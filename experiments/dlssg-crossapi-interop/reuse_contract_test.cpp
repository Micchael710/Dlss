#include "../../wisteria/native/dlssg/reuse_contract.h"
#include <iostream>
int main(){int rejected=0;auto reject=[&](auto f){try{f();}catch(const std::runtime_error&){++rejected;return;}throw std::runtime_error("Unsafe reuse accepted");};
 reject([]{integration::requireSlotRetired(true,false,false);});reject([]{integration::requireSlotRetired(false,true,false);});reject([]{integration::requireD3DDone(1,2);});reject([]{integration::requireD3DDone(UINT64_MAX,2);});
 integration::requireD3DDone(2,2);integration::requireSlotRetired(false,false,false);std::cout<<"PASS native slot/command retirement: "<<rejected<<" unsafe cases rejected\n";}
