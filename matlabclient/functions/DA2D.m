function [Ra, dia,Du] = DA2D(R,epos)
    % 
    % clear all; close all;
    % dx = (0:6)'*0.5;
    % dy = (0:2)'*0.5;
    % epos = [repelem(dy',7);repmat(dx',1,3)];
    % epos = [repelem(dx',3);repmat(dy',1,7)];
    % 
    % %1st row are y coords, 2nd row are x-coords
    % epos = [repmat(dy',1,7);repelem(dx',3)];
    % 
    % epos = 2*epos;

    [D,Du] = darray(epos);
    
    dia = -flipud(Du);

    minx = min(real(Du));
    maxx = max(real(Du));
    miny = min(imag(Du));
    maxy = max(imag(Du));

    %Dux  = unique(real(Du));
    %Duy  = unique(imag(Du));
    
    Nd   = length(Du);
    Ra   = complex(zeros(Nd));
    for q= 1:Nd
        for r = q:Nd
            i = Nd+q-r;
            Ra(q,r) = mean(R(Du(i)==D));
        end
    end

    Ra = triu(Ra,1)+Ra';
end