function [diff,diffuniq] = darray(epos)
    %2D difference co-array from Matlab steervec compatible element-
    %position vector epos
    
    %outputs gaussian integer matrices diff & diffuniq
    % real part = x pos, imaginary part = ypos

    Ny = length(unique(epos(1,:)));
    Nx = length(unique(epos(2,:)));

    if(Nx*Ny ~= size(epos,2))
        disp('duplicate sensor positions');
    end

    %store differences as gaussian integers:
    Dc = zeros(Ny,Nx);
    for n=1:size(epos,2)
        Dc(Ny-epos(1,n),epos(2,n)+1) = floor(epos(2,n) + 1j*epos(1,n));
    end

    Dct = Dc;

    diff = NaN(Ny,Nx,Ny*Nx);
    for ix = 1:Nx
        for iy = 1:Ny
            diff(:,:,Ny*(ix-1)+iy) = Dc(iy,ix) - Dct;
        end
    end

    diffuniq =  unique(diff);

    %scatterplot(diffuniq);
end