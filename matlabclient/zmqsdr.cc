//Coherent RTL-SDR mex interface for matlab.
#include <iostream>
#include <zmq.hpp>
#include "mex.hpp"
#include "mexAdapter.hpp"
#include <memory>

using matlab::mex::ArgumentList;
using namespace matlab::data;
using matlab::engine::convertUTF8StringToUTF16String;
using matlab::engine::convertUTF16StringToUTF8String;


const int timeout_ = 1000;

const uint32_t CONTROLMSG_MAGIC=(('C'<<24) & ('T'<<16) & ('R'<<8) & 'L');
const uint32_t CONTROLMSG_SETFCENTER=0x01;
const uint32_t CONTROLMSG_QUERYLAGS =0x02;

static bool initialized=false;

class MexFunction : public matlab::mex::Function {
private:
        zmq::context_t      context;
        zmq::socket_t       socketr, socketc;
        zmq::message_t      msgr;
        std::string         addressr;
        std::string         addressc;

        std::shared_ptr<matlab::engine::MATLABEngine> matlabPtr;
        std::ostringstream  stream;
        ArrayFactory        f;

        uint64_t            rows,cols; //complaints about narrowing conv. matlab stores matrix sizes in long unsigned int...
        struct args{
            char     op;
            uint32_t centerfreq;
            std::string ip,port,cport;
        }args;
        //matlab::data::TypedArray<std::complex<float>> *out;
        //TypedArray<std::complex<float>> *out;

    void resizematrix(uint64_t r, uint64_t c){
        rows = r;
        cols = c;
        //mexUnlock();
        //outmatrix->release(); //clear old
        //*out = f.createArray<std::complex<float>>({rows,cols},{0});
        //mexLock();
    }

public: 

    MexFunction(): context(1),socketr(context,ZMQ_SUB),socketc(context,ZMQ_DEALER){
         matlabPtr = getEngine();
         rows=2;
         cols=2;
         //mexLock();
         //*outmatrix = f.createArray<std::complex<float>>({rows,cols},{0}); //this will crash? lock?
    }
    ~MexFunction(){
        //mexUnlock();
        //outmatrix->release();
    }

    void convoutputmtx(int8_t *src, matlab::data::TypedArray<std::complex<float>> &dest){
        float scaler = (1.0f/128.0f);
        int n=0;
       
        //this iterator approach is slow...
        for(auto d : dest){
            d = scaler * std::complex<float>(((int8_t)(src[n])),((int8_t)(src[n + 1])));
            n+=2;
        }
    }

    void convoutputmtxraw(int8_t *src, std::complex<float> * dest, int N){
        float scaler = (1.0f/128.0f);
        for(int n=0;n<N;n++)
                dest[n] = scaler * std::complex<float>(((int8_t)(src[n])),((int8_t)(src[n + 1])));
    }

    void operator()(ArgumentList outputs, ArgumentList inputs) {

        parseArguments(inputs);
        

        switch (args.op) {
            case 'i':
                initzmq(args.ip,args.port,args.cport);
                outputs[0] = f.createScalar<double>(1);
            break;
            case 'r':
                {
                    if (socketr.recv(&msgr)==EAGAIN){
                        outputs[0] = f.createScalar<double>(-1);
                        
                    }
                    else{
                        int8_t *data = static_cast<int8_t *>(msgr.data());

                        uint32_t gseq = *((uint32_t *) data);
                        uint32_t c    = *(((uint32_t *) data)+1);
                        uint32_t r    = *(((uint32_t *) data)+2);

                        float scaler=1.0f/128.0f;
                        int offset=16+sizeof(uint32_t)*cols;
                        int n=0;
                        
                        if (inputs.size()>1){
                        ArrayDimensions dimensions = inputs[1].getDimensions();

                            if ((dimensions[0]==r) && (dimensions[1]==c)){
                                matlab::data::TypedArray<std::complex<float>> out = std::move(inputs[1]);                            
                                convoutputmtx(data+offset,out);
                                //int N = out.getNumberOfElements();
                                //auto outptr = out.release();
                                //convoutputmtxraw(data+offset,outptr.get(),N);
                                //out = f.createArrayFromBuffer<std::complex<float>[]>(dimensions,outptr);

                                outputs[0] = std::move(out); //sampledata
                            }
                            else{
                                matlab::data::TypedArray<std::complex<float>> out = f.createArray<std::complex<float>>({r,c},{0}); //if output size does not match, allocate a new matrix.
                                convoutputmtx(data+offset,out);
                                outputs[0] = std::move(out); //sampledata
                            }
                        }
                        else{                          
                            matlab::data::TypedArray<std::complex<float>> out = f.createArray<std::complex<float>>({r,c},{0}); //if output size does not match, allocate a new matrix.
                            convoutputmtx(data+offset,out);
                            outputs[0] = std::move(out); //sampledata
                        }
                                                                      
                        //create the channel readcount vector:
                        matlab::data::TypedArray<uint32_t> seq = f.createArray<uint32_t>({1,c},{0});
                        for (int n=0;n<c; n++)
                            seq[0][n] = *(((uint32_t *) data) + 4 + n);

                        
                        outputs[1] = f.createScalar<uint32_t>(gseq); //global sequence num
                        outputs[2] = std::move(seq); //channel sequences...

                    }
                }
            break;
            case 't':
                stream << "retuning to" << std::to_string(args.centerfreq) << std::endl;
                displayOnMATLAB(stream);
                retune(args.centerfreq);
            break;
            case 'c':

            break;
            default:
            break;
        }

    }

    void retune(uint32_t fcenter){
        zmq::message_t message(sizeof(uint32_t)*3);
        uint32_t *msg =  static_cast<uint32_t*>(message.data());

        msg[0]=CONTROLMSG_MAGIC;
        msg[1]=CONTROLMSG_SETFCENTER;
        msg[2]=fcenter;
        
        socketc.send(message);
    }

    void initzmq(std::string ip, std::string p, std::string cp) { //, std::string port, std::string cport){
        
        std::string addr = "tcp://" + ip + ":" + p;
        std::string caddr= "tcp://" + ip + ":" + cp;

        stream << "receiver address:" << addr << std::endl;
        displayOnMATLAB(stream);
        stream << "control address:" << caddr << std::endl;
        displayOnMATLAB(stream);
        
        int timeout=timeout_;

        socketr.setsockopt(ZMQ_RCVTIMEO,&timeout,sizeof(int));
        socketr.connect(addr.data());

        socketr.setsockopt(ZMQ_SUBSCRIBE,"",0);

        socketc.connect(caddr.data());
    }

    // read inputs into struct args, perform some checking against invalid input:
    void parseArguments(ArgumentList inputs){
            
            int numargs = inputs.size();

            if (numargs>0){
                if(inputs[0].getType()!= ArrayType::CHAR)
                  matlabPtr->feval(u"error",0, std::vector<Array>({f.createScalar("Incorrect input 0, operation char ('i','r','t','c').")}));
                const CharArray oper = inputs[0];
                args.op = oper[0];
            }
            if (numargs>1){
            /*    if(inputs[1].getType()!= ArrayType::DOUBLE)
                    matlabPtr->feval(u"error",0, std::vector<Array>({f.createScalar("Incorrect input: Center frequency should be double type")}));
                TypedArray<double> cfreq = inputs[1];
                if (!cfreq.isEmpty())
                    args.centerfreq = cfreq[0];*/
            }
            if (numargs==5){
                if ((inputs[2].getType()!= ArrayType::CHAR)||(inputs[3].getType()!= ArrayType::CHAR)||(inputs[4].getType()!= ArrayType::CHAR))
                    matlabPtr->feval(u"error",0, std::vector<Array>({f.createScalar("Incorrect input: ip, port & control port should be char strings")}));
                
                args.ip    = std::string(((CharArray) inputs[2]).toAscii());
                args.port  = std::string(((CharArray) inputs[3]).toAscii());
                args.cport = std::string(((CharArray) inputs[4]).toAscii());
            }

    }

    //from matlab examples:
    void displayOnMATLAB(std::ostringstream& stream) {
        // Pass stream content to MATLAB fprintf function
        matlabPtr->feval(u"fprintf", 0,
            std::vector<Array>({ f.createScalar(stream.str()) }));
        // Clear stream buffer
        stream.str("");
    }
};
