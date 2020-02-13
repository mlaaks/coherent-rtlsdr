% 
% ang = [0;0];
% 
% ang_rad = [ang(1);ang(2)+90]*pi/180;
% 
dx = (0:6)'*0.5;
dy = (0:2)'*0.5;

epos = [repelem(dy',7);
        repmat(dx',1,3)];
% 
% a = steervec(epos,ang);
% 
% 
% ax = exp(1j*2*pi*dx*cos(ang_rad(1))*cos(ang_rad(2)));
% ay = exp(1j*2*pi*dy*sin(ang_rad(1))*cos(ang_rad(2)));
% 
% aa = kron(ay,ax);
% 
% sum(a-aa)
% 
% A = [a aa];

load('testmat.mat');
x = x - mean(x);

[U,~,~] = svd(x(:,2:end)'*x(:,2:end));
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