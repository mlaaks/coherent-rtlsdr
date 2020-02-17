function [epos] = D2Dtoepos(Du)
    minx = min(real(Du));
    maxx = max(real(Du));
    miny = min(imag(Du));
    maxy = max(imag(Du));
   
    epos = zeros(maxy-miny+1,maxx-minx+1);
    
    dx   = unique(real(Du));
    dy   = unique(imag(Du));
    
    epos = [repmat(dy',1,length(dx));repelem(dx',length(dy))];
end