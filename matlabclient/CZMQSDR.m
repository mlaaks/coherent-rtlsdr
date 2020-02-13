%Coherent-RTL-SDR
%
%The CZMQSDR system object. This interfaces to the zmqsdr.c mex
%implementation.

classdef CZMQSDR < matlab.System
    properties %(Access = private)
      CenterFrequency = 1024e6;
      IPAddress ='127.0.0.1'; %'127.0.0.1', 5555
      Port = '5555';
      ControlPort = '5556';
      RefNoiseEnabled = false;
      setuppassed = 'uninitialized';
    end
   
   methods
       function obj = CZMQSDR(varargin)
            setProperties(obj,nargin,varargin{:});
       end
       
       function enablerefnoise(~,state)
           if (state)
                %disp('refnoise enable');
                zmqsdr('e');
            else
                zmqsdr('d');
                %disp('refnoise disable');
            end
       end
       function commit(~)
       
       end
   end
   
   methods(Access = protected)
       
%        function resetImpl(obj)
%             obj.CenterFrequency = 1024e6;
%             obj.IPAddress ='127.0.0.1'; %'127.0.0.1', 5555
%             obj.Port = '5555';
%             obj.ControlPort = '5556';
%             obj.RefNoiseEnabled = false;
%        end
       
       function validatePropertiesImpl(obj)
         if (obj.CenterFrequency<24e6) || (obj.CenterFrequency>1766e6)
            error('Property CenterFrequency out of range [24,1766] MHz.');
         end
       end
       
       
      function processTunedPropertiesImpl(obj)
        if (obj.CenterFrequency<24e6) || (obj.CenterFrequency>1766e6)
           error('Property CenterFrequency out of range [24,1766] MHz.');
        end
        
        if isChangedProperty(obj,'CenterFrequency')
          zmqsdr('t',obj.CenterFrequency); %retune (t)
        end
        
        if isChangedProperty(obj,'RefNoiseEnabled')
            if (obj.RefNoiseEnabled)
                disp('refnoise enable');
                zmqsdr('e');
            else
                zmqsdr('d');
                disp('refnoise disable');
            end
        end
        
      end
      
      function setupImpl(obj)
          ret = zmqsdr('i',obj.CenterFrequency,obj.IPAddress,...
          obj.Port,obj.ControlPort); % init (i)
          if (ret==0)
            obj.setuppassed = 'Setup complete';
          else
            obj.setuppassed = 'Init error';
            disp(obj.setuppassed);
          end
      end
      function [y,gseq,seq] =stepImpl(varargin)
          timeoutctr = 0;
          while(1)
            if length(varargin)<2
                [y,gseq,seq]=zmqsdr('r'); % receive (r)
            else
                [y,gseq,seq]=zmqsdr('r',varargin{2}); % receive (r)
            end
            if y~=-1
                break;
%a socket timeout (250 ms) returns control here. Allows matlab to pause, 
% & regain control if needed. If timeouted, retry.
            else      
                timeoutctr = timeoutctr+1;
                if(timeoutctr>10)
                    disp('timeout, no response from radio');
                    timeoutctr=0;
                end
            end
          end
      end
      
      function releaseImpl(~)
          zmqsdr('c'); % clear (c)
          clear mex;
      end
   end
end