clear all;
close all;

nframes = 10;

sdr = CZMQSDR('IPAddress','127.0.0.1');

for n=1:nframes
   [rec(:,:,n),gseq(n),seq(n,:)]=sdr();
end

 Np = size(rec(:,:,1),2);
 Nr = round(Np/4);

%% PLOT:
for n=1:nframes
     for k=1:Np
        subplot(Nr,4,k);
        c=xcorr(rec(:,1,n),rec(:,k,n));
        stem(c.*conj(c));
        xlim([0 (2*size(rec,1)-1)]);
        abs(max(c))/rms(c);
        title(num2str(seq(n,k)));
     end
     pause(0.01);
end

release(sdr);