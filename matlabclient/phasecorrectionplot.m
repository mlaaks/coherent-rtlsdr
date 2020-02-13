%Coherent-RTL-SDR
%
%Matlab script for checking the phase correction.

%Read output from the phase correction debug port. needs plain zmq, get:
%zeromq-matlab 
%clone https://github.com/smcgill3/zeromq-matlab
%build ( run make), if succesfuly, copy the resulting zmq.mexa64 to
%your matlab path.
clear all; close all;

%set the time to capture data:
runforminutes=30;

FESR=1024000;
FESR=1e6;
L= 2^14;
tb=L*(1/FESR);



mq = zmq('subscribe', 'tcp', '127.0.0.1', 5557);
loop_n=round(runforminutes*60/tb);

recz=[];
for n=1:loop_n
     pcorr = zmq('receive', mq);
     re_im = reshape(typecast(pcorr, 'single'), 2, length(pcorr)/8);
     z = complex(re_im(1,:), re_im(2,:));
     
     recz=[recz; z];
end

%%
%load('onehour.mat');
t=(0:(length(recz)-1))*tb/60;
p=180/pi*angle(recz(:,1:end));

%try linear regression plots
% for nn=1:size(p,2)
%     b(nn,:)=polyfit(t.',p(:,nn),1);
%     lr(:,nn) = b(nn,1).*t+b(nn,2);
% end

plot(t,p);
hold on;
%plot(t,lr,'k');
xlabel('time [min]','FontSize',24);
ylabel('phasecorrection [degree]','FontSize',24);
xlim([0 runforminutes]);
hold off;

%plot deviations
% devs = std(p) %sqrt(var(p))
% figure
% bar(devs)
% xlabel('dongle #');
% ylabel('stdev');
% 
% %plot regression coeff
% figure
% bar(b(:,1))
% ylim([-2,2]);
% xlabel('dongle #');
% ylabel('rad/min');



