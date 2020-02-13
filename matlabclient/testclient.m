clear all;
close all;

sdr = CZMQSDR('IPAddress','127.0.0.1');
%sdr = CZMQSDR('IPAddress','10.10.10.10');

recs = ones(1,22);
FESR = 1e6; % 2048000;
scope = dsp.SpectrumAnalyzer(...
    'Name',             'Spectrum',...
    'Title',            'Spectrum', ...
    'SpectrumType',     'Power',...
    'FrequencySpan',    'Full', ...
    'SampleRate',        FESR, ...
    'YLimits',          [-50,5],...
    'SpectralAverages', 50, ...
    'FrequencySpan',    'Start and stop frequencies', ...
    'StartFrequency',   -500e3, ...
    'StopFrequency',    500e3);
rec=[];

tic;
t1=toc;
recsmp =[];
for n=1:100
   if(n==10)
       sdr.enablerefnoise(true);
   end
   if(n==20)
       sdr.enablerefnoise(false);
   end
   
   [rec,gseq,seq]=sdr();
   recsmp = [recsmp; rec];   
end
t2=toc;
fprintf('time was %f \n', t2-t1);
fprintf('nominal time should be %4.2f, diff: %4.0f percent \n', 1/(FESR)*size(rec,1)*n, (t2-t1)/(1/FESR*size(rec,1)*n)*100);

release(sdr); % this needs to be called. otherwise data can be relatively old.
%%
%mq = zmq('subscribe', 'tcp', '127.0.0.1', 5555);
%pcorr = zmq('receive', mq);

