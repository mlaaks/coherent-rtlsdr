%Coherent-RTL-SDR
%
%Matlab script for 2D beamforming measurements on a 7 X 3 element URA
%868 MHz ISM band
%
%Receive one frame, save the data for later use, plot the 2D Music
%pseudo spectrum

clear all;close all;

addpath('functions');

%set save filename and path prefix
fpath = 'data/';
fname = 'meas';


%Matlab steervec() compatible element position matrix:
dx = (0:6)'*0.5;
dy = (0:2)'*0.5;
epos=[repmat(dy',1,7);repelem(dx',3)];
    


sdr = CZMQSDR('IPAddress','127.0.0.1');

%switch on refnoise, causes phasecoefficients to recalibrate, wait and
%disable
%sdr(); % receive one frame, otherwise the sysobj is not initialized.
%sdr.enablerefnoise(true);
%pause(0.5);
%sdr.enablerefnoise(false);
%pause(0.5);
%reinit, otherwise we get old data from zmq buffers.
%release(sdr);
%sdr = CZMQSDR('IPAddress','127.0.0.1');

%receive one frame
[X,gseq,seq] = sdr();

%save and close
spth = spath(fpath,fname);
fprintf('SAVING RECEIVED MATRIX TO FILE: %s \n',spth);
save(spth,'X');
release(sdr);

%2d DOA plot
x = X(:,2:end);
x = x - mean(x);
pmusicplot(x,epos,1,1);
figure;
pmusicplot(x,epos,1,0);

% filename with a running counter, returns a filename&path where
% the filename is : highest fname + 1
function [o] = spath(fpath,fname)
    try
        lst  = ls([fpath '*' fname '*']);
        k    = strfind(lst,fname);
        nmax = 0;

        for nn=1:length(k)
            nstart = k(nn)+length(fname);
            k2 = strfind(lst(nstart:end),'.mat');
            nend = k2(1)-2;
            nvec(nn) = str2num(lst(nstart:nstart + nend));
        end
    
        o = [fpath fname num2str(max(nvec)+1) '.mat'];
    catch ME
        o = [fpath fname '0.mat']; 
    end
end