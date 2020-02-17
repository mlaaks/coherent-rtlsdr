%Coherent-RTL-SDR

%Analyze results saved by measurement_script in parent...
%addpath(genpath('functions'))
addpath('../functions');

%Matlab steervec() compatible element position matrix:
dx = (0:6)'*0.5;
dy = (0:2)'*0.5;
epos=[repmat(dy',1,7);repelem(dx',3)];


for n=1:24
    load(['meas' num2str(n) '.mat']);

    X = X(:,2:end);

    pmusicplot(X,epos,1);  %set DA=1 for direct aug. difference co-array
    drawnow;
    %pause(1);
end