%Coherent-RTL-SDR

%Analyze results saved by measurement_script in parent...

%uses Data space to figure units converter from mathworks:
%https://se.mathworks.com/matlabcentral/fileexchange/10656-data-space-to-figure-units-conversion


%addpath(genpath('functions'))
clear all; close all;
addpath('../functions');

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
    'StartFrequency',   -FESR/2, ...
    'StopFrequency',    FESR/2);

mdatab1  =[8:23;
          -5*ones(1,16);
          4.7*ones(1,16);
          -0.71*ones(1,16)];

mdatab2 =[44,45,46,47;
         -5*ones(1,4);
         4.7*ones(1,4);
         -0.71*ones(1,4)];
     
mdatac1 =[4,5,6,7;
         -2.5*ones(1,4);
         6.6*ones(1,4);
         -0.3*ones(1,4)];
          
mdatac2  =[24,25,26,27;
         -2.5*ones(1,4);
         6.6*ones(1,4);
         -0.31*ones(1,4);];
     

%furthest tx position, table at the window corner     
mdatafar = [32,33,34,35;
          0*ones(1,4);
          7.5*ones(1,4);
          -0.71*ones(1,4)];

%two measurements quite close.      
mdatanear = [36:39;
             0*ones(1,4);
             2.75*ones(1,4);
             -0.71*ones(1,4)];
mdatanearb= [40:43;
             0*ones(1,4);
             4.4*ones(1,4);
             -0.71*ones(1,4)];
         
%Choose dataset and calculate true doas.     
mdata = mdatafar;

truedoa = (180/pi)* atan([(mdata(2,:))./mdata(3,:);
                          mdata(4,:)./mdata(3,:)]);
                     

%Matlab steervec() compatible element position matrix:
dx = (0:6)'*0.5;
dy = (2:-1:0)'*0.5;
epos=[repmat(dy',1,7);repelem(dx',3)];

b = fir1(128,1/8);


%number of signals
K=1;
%use direct augmentation
DA=1;

for nn= 1:length(mdata)
    load(['meas' num2str(mdata(1,nn)) '.mat']);

    if (sum(sum(diff(seq)~=ones(9,22)))~=0)
        fprintf('WARNING rcnt glitch in meas%d\n',num2str(mdata(1,nn)));
    end
    
    X = X(:,end:-1:2);
    
    X = filter(b,1,X); %limit bandwidth
    scope(X);

    [P,Nx,Ny] = pmusic(X,epos,K,DA);
    
    %find the peak:
    [m,idx]  = max(P(:));
    [idxx idxy] = ind2sub(size(P),idx);
    
    alphas = -90:90; betas  = -90:90;
    clf;
    imagesc(alphas,betas,10*log10(P)); colorbar;
    
    %add the true DOA as annotation
    xa        = [truedoa(1,nn) truedoa(1,nn)-1];
    ya        = [truedoa(2,nn) truedoa(2,nn)-1];
    
    [xaf,yaf] = ds2nfu(xa,ya); %see link in header.
    annotation('textarrow',xaf,yaf,'String','TRUE','Color','red');
    

   % annotation('textarrow',[0.128 0.128],...
   %[0.105 0.105],'String','TRUE','Color','red');

    minn = 10*log10(min(P,[],'all'));
    ttl = sprintf('%d X %d array. Peak location: %d, %d. Nse floor: %d dB\n True doa: %d, %d',Nx,Ny, ...
          round(idxx-91), round(idxy-91),round(minn),round(truedoa(1,nn)),round(truedoa(2,nn)));
    title(ttl);
    xlabel('Azimuth [deg]');
    ylabel('Elevation [deg]');
    
    drawnow;
    pause(1);
end