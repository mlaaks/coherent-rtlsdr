%Coherent-RTL-SDR
%
%Plot the MUSIC pseudo spectrum.
% x =  Ns * Nsensor input sample matrix
% epos = matlab steervec() compatible element position matrix
% K = number of signals
% DA = 1, Do direct augmentation (co-array processing)


function pmusicplot(x,epos,K,DA)
    miny = -20;
    %sample covariance mtx;
    Ns= size(x,1);
    x = x - mean(x);
    R = x'*x/Ns;
    
    Nx = numel(unique(epos(2,:)));
    Ny = numel(unique(epos(1,:)));

    %direct augmentation:
    if (DA)
        [~,Du]   = darray(2*epos);
        [R,~,Du] = DA2D(R,2*epos);
        Nx       = numel(unique(real(Du)));
        Ny       = numel(unique(imag(Du)));
    end
    [U,~,~] = svd(R);
    Un      = U(:,(K+1):end);

    alphas = -90:90; betas  = -90:90;
    for alpha=alphas
        for beta = betas
            ang = [beta;alpha];
            if(DA)
                a = steervec(0.5*D2Dtoepos(Du),ang);
            else
                a = steervec(epos,ang);
            end
            P(alpha+91,beta+91) = (vecnorm(a)/vecnorm(Un'*a))^2;
        end
    end
    P = P*(1/max(max(P)));
  
    %find the peak:
    [m,idx]  = max(P(:));
    [idxx idxy] = ind2sub(size(P),idx);

    surf(alphas,betas,10*log10(P));

    minn = 10*log10(min(P,[],'all'));
    ttl = sprintf('%d X %d array. Peak location: %d, %d. Nse floor: %d dB\n',Nx,Ny, ...
          round(idxx-91), round(idxy-91),round(minn));
    title(ttl);

    %title([num2str(round(idxx-91)) ',' num2str(round(idxy -91))]);
    miny = min(minn,miny);
    zlim([miny 1]);
end
