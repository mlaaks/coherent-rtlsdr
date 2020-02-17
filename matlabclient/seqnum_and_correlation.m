%Coherent-RTL-SDR
%
%Matlab script to validate timing synchronization. 
%
%Record a number of frames, plot correlations to reference noise 
%on channel 0 (=first column of X);
clear all; close all;

nframes = 10;
ymax    = 2000;

%open the device and read samples
sdr = CZMQSDR('IPAddress','127.0.0.1');

for n=1:nframes
   [X(:,:,n),gseq(n),seq(n,:)]=sdr();
end

release(sdr);

%% PLOT the cross-correlations:
% when synchronized, the channel should have a noticeable peak in the
% center. Top left is the reference noise ch and thus contains 
%no useful info.

Np = size(X(:,:,1),2);
Nr = round(Np/4);

figure('units','normalized','outerposition',[0 0 1 1]);
for n=1:nframes
     for k=1:Np
        subplot(Nr,4,k);
        c=xcorr(X(:,1,n),X(:,k,n));
        stem(c.*conj(c));
        xlim([0 (2*size(X,1)-1)]);
        ylim([0 ymax]);
        
        %PAPR. Crest factor could also be used.
        PAPR = abs(max(c))^2/rms(c)^2;
        title(['seq: ' num2str(seq(n,k)) ' PAPR: ' num2str(PAPR)]);
     end
     pause(0.01);
end
