%Coherent-RTL-SDR
%
%Validate the operation of reference noise switching from Matlab


clear all;close all;
names = {'ref','s0','s1'};
%ZMQ client for sampledata, a matlab system object:
sdr = CZMQSDR('IPAddress','127.0.0.1');
 
% receive one frame and throw it away. This is currently 
% necessary, otherwise the system object does not initialize 
% properly and will result in crash, when enablerefnoise is called
% before receiving.
sdr();

%disable reference noise
sdr.enablerefnoise(false);

recsmp =[];
%loop, switch noise on for ~10 frames
for n=1:100
   if(n==10)
       sdr.enablerefnoise(true);
   end
   if(n==19)
       sdr.enablerefnoise(false);
   end
   %receive
   [rec,gseq,seq]=sdr();
   recsmp = [recsmp; rec];   
end
release(sdr);

%plot pwr w.r.t. time:
M = size(rec,2);
for n=1:M
    subplot(M,1,n);
    plot(recsmp(:,n).*conj(recsmp(:,n)))
    title(['s' num2str(n-1)]);
    ylabel('pwr');
    xlabel('n [sample index]');
    xlim([0,size(recsmp,1)-1]);
end
