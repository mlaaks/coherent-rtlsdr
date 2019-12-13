#include "common.h"
bool is_zeros(uint32_t *p,int n){
    bool res=true;
    for(int k=0;k<n;++k){
        if (p[k]!=0)
        {
            res=false;
            break;
        }
    }
    return res;
}