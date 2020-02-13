clear all;close all;

%set save filename and path prefix
fpath = 'data/';
fname = 'meas';

sdr = CZMQSDR('IPAddress','127.0.0.1');

%switch on refnoise, causes phasecoefficients to recalibrate, wait and
%disable
sdr(); % receive one frame, otherwise the sysobj is not initialized.
sdr.enablerefnoise(true);
pause(0.5);
sdr.enablerefnoise(false);
pause(0.5);
%reinit, otherwise we get old data from zmq buffers.
release(sdr);
sdr = CZMQSDR('IPAddress','127.0.0.1');

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
pmusicplot(x);

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


%Plot the 2d pseudo spectrum as a surface plot
function pmusicplot(x)
    dx = (0:6)'*0.5;
    dy = (0:2)'*0.5;
    epos = [repelem(dy',7);
            repmat(dx',1,3)];

    x = x - mean(x);

    [U,~,~] = svd(x'*x);
    Un      = U(:,2:end);

    alphas = -90:90; betas  = -90:90;
    for alpha=alphas
        for beta = betas
            ang = [beta;alpha];
            a = steervec(epos,ang);
            P(alpha+91,beta+91) = (vecnorm(a)/vecnorm(Un'*a))^2;
        end
    end
    P = P*(1/max(max(P)));

    surf(alphas,betas,10*log10(P));
    zlim([-20 1]);
end
