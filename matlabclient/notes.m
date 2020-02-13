clear all; close all;
N=10000;
x=1000*(randn(N,1) +j*randn(N,1));
%x= sin(2*pi*20*0:N);
%y=randn(N,1) +j*randn(N,1);

ford = 8;

k=1;
del=(0:0.01:0.5);
for k=1:length(del)
    d = fdesign.fracdelay(del(k),'N',ford);
    hd = design(d,'lagrange','filterstructure','farrowfd');

    y1 = filter(hd,x);
    y1 = y1(ford/2+1:end);
    x  = x(1:end-ford/2-1);
    %y1 = x;

    c = xcorr(x,y1);

    %plot(abs(c));
    cmax= c(N-1);
    csq = c.*conj(c);
    [m,idx] = max(csq);
    cre = real(c);
    %c = sqrt(real(c));
    ang = angle(c(idx))/(2*pi);

    a= - cre(idx-1) - 2*cre(idx) + cre(idx+1);
    b= - 0.5*cre(idx-1) +0.5*cre(idx+1);
    D(k)=b/a;
end
%%
plot(del,D*2.8669);
X=[ones(length(D),1) D'];
Y=del';
X\Y

%fprintf('inverse interpol: %f, angle %f, %f\n',D*(1+pi/2),ang/D,del/D);
%fprintf('%f\n',(real(c(idx))/imag(c(idx)))/N);
%plot(real(x(1:20)))
%hold on
%plot(real(y1(1:20)))