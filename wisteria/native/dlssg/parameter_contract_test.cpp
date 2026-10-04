// CPU fixture for the public NGX helper's count/index/resource binding.
// No NGX runtime, community module, GPU, or generated pixels are loaded.
#include <windows.h>
#include <d3d12.h>
#include <nvsdk_ngx_helpers_dlssg_d3d.h>
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>
static std::map<std::string,unsigned> integers;
static std::map<std::string,ID3D12Resource*> resources;
static unsigned expectedIndex, expectedCount, calls;
static ID3D12Resource *expectedOutput, *expectedStatus, *expectedInput;
static void require(bool value){if(!value)throw std::runtime_error("Public helper binding mismatch");}
void NVSDK_CONV NVSDK_NGX_Parameter_SetUI(NVSDK_NGX_Parameter*,const char* name,unsigned value){integers[name]=value;}
void NVSDK_CONV NVSDK_NGX_Parameter_SetF(NVSDK_NGX_Parameter*,const char*,float){}
void NVSDK_CONV NVSDK_NGX_Parameter_SetVoidPointer(NVSDK_NGX_Parameter*,const char*,void*){}
void NVSDK_CONV NVSDK_NGX_Parameter_SetD3d12Resource(NVSDK_NGX_Parameter*,const char* name,ID3D12Resource* value){resources[name]=value;}
NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_EvaluateFeature_C(ID3D12GraphicsCommandList*,const NVSDK_NGX_Handle*,const NVSDK_NGX_Parameter*,PFN_NVSDK_NGX_ProgressCallback_C){
    require(integers.at(NVSDK_NGX_DLSSG_Parameter_MultiFrameCount)==expectedCount);
    require(integers.at(NVSDK_NGX_DLSSG_Parameter_MultiFrameIndex)==expectedIndex);
    require(resources.at(NVSDK_NGX_DLSSG_Parameter_OutputInterpolated)==expectedOutput);
    require(resources.at(NVSDK_NGX_DLSSG_Parameter_OutputDisableInterpolation)==expectedStatus);
    require(resources.at(NVSDK_NGX_DLSSG_Parameter_Backbuffer)==expectedInput);
    ++calls;return NVSDK_NGX_Result_Success; // Mock API result only; not generation evidence.
}
int main(){
    int realFixture{},outputs[4]{},statuses[4]{};
    expectedInput=reinterpret_cast<ID3D12Resource*>(&realFixture);
    for(unsigned count=1;count<=4;++count){
        expectedCount=count;
        for(unsigned slot=0;slot<count;++slot){
            expectedIndex=slot+1;
            expectedOutput=reinterpret_cast<ID3D12Resource*>(&outputs[slot]);
            expectedStatus=reinterpret_cast<ID3D12Resource*>(&statuses[slot]);
            if(slot){require(expectedOutput!=reinterpret_cast<ID3D12Resource*>(&outputs[slot-1]));require(expectedStatus!=reinterpret_cast<ID3D12Resource*>(&statuses[slot-1]));}
            NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};
            ep.pBackbuffer=expectedInput;ep.pOutputInterpFrame=expectedOutput;ep.pOutputDisableInterpolation=expectedStatus;
            NVSDK_NGX_DLSSG_Opt_Eval_Params opts{};opts.multiFrameCount=count;opts.multiFrameIndex=slot+1;
            require(NGX_D3D12_EVALUATE_DLSSG(nullptr,nullptr,nullptr,&ep,&opts)==NVSDK_NGX_Result_Success);
        }
    }
    require(calls==10);
    std::cout<<"PASS CPU MOCK: public helper count1..4, API index=slot+1, distinct output/status binding, same real input; no GPU generation asserted\n";
}
