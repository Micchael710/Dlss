// Public Streamline capability/options only. No rendering, Present, Vulkan or NGX-direct.
#include <windows.h>
#include <tlhelp32.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <sl.h>
#include <sl_dlss_g.h>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
static const char* stage="PREFLIGHT";
static void loadedModules() {
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    if(snapshot==INVALID_HANDLE_VALUE)return;
    MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Module32FirstW(snapshot,&entry))do{std::wcout<<L"LOADED_MODULE="<<entry.szExePath<<std::endl;}while(Module32NextW(snapshot,&entry));
    CloseHandle(snapshot);
}
template<class T> T* api(HMODULE module,const char* name) {
    auto fn=reinterpret_cast<T*>(GetProcAddress(module,name));
    if(!fn)throw std::runtime_error(std::string("Missing public export ")+name);
    return fn;
}
static void result(const char* key,sl::Result r) {
    std::cout<<key<<"="<<static_cast<unsigned>(r)<<std::endl;
    if(r!=sl::Result::eOk)throw std::runtime_error(std::string(key)+" failed");
}
static void hr(HRESULT r) {
    std::cout<<"HRESULT=0x"<<std::hex<<static_cast<unsigned>(r)<<std::dec<<std::endl;
    if(FAILED(r))throw std::runtime_error("D3D12/DXGI failure");
}
int wmain(int argc,wchar_t** argv) {
    HMODULE interposer{},proxy{};PFun_slShutdown* shutdown{};bool initialized=false;
    ComPtr<ID3D12Device> device; // Keep device alive through failure-path slShutdown.
    // Runtime directory must contain unmodified, pre-staged authorized components.
    // Never search PATH, download, fabricate support, or generate proxy configuration.
    try {
        if(argc!=3)throw std::runtime_error("Usage: executable absolute-runtime-directory absolute-log-directory");
        fs::path dir=fs::absolute(argv[1]);
        fs::path logDir=fs::absolute(argv[2]);
        std::cout<<"STREAMLINE_HEADER_VERSION="<<SL_VERSION_MAJOR<<'.'<<SL_VERSION_MINOR<<'.'<<SL_VERSION_PATCH<<std::endl;
        for(const auto* name:{L"sl.interposer.dll",L"sl.common.dll",L"sl.dlss_g.dll",L"sl.reflex.dll",L"nvngx_dlssg.dll",L"component/version.dll"}) {
            if(!fs::is_regular_file(dir/name)) {
                std::wcout<<L"MISSING_COMPONENT="<<name<<std::endl;
                std::cout<<"SM86_PROXY_LOADED=NO\nSM86_PROXY_ACTIVE=NO\nFAIL_STAGE=MISSING_AUTHORIZED_RUNTIME_BINARIES"<<std::endl;
                return 2;
            }
        }
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        auto cookie=AddDllDirectory(dir.c_str());
        if(!cookie)throw std::runtime_error("Runtime directory registration failed");
        stage="SM86_LOAD";
        // Reuse ExternalLoader's proven absolute LoadLibraryW + adjacent INI layout.
        proxy=LoadLibraryW((dir/L"component/version.dll").c_str());
        std::cout<<"SM86_PROXY_LOADED="<<(proxy?"YES":"NO")<<std::endl;
        if(!proxy)throw std::runtime_error("SM86 LoadLibrary failed");
        // Loaded is not proof that interception was applied: keep these separate.
        std::cout<<"SM86_PROXY_ACTIVE=NOT_INDEPENDENTLY_VERIFIED"<<std::endl;
        stage="STREAMLINE_LOAD";
        interposer=LoadLibraryExW((dir/L"sl.interposer.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!interposer)throw std::runtime_error("Streamline LoadLibrary failed");
        auto init=api<PFun_slInit>(interposer,"slInit");shutdown=api<PFun_slShutdown>(interposer,"slShutdown");
        auto supported=api<PFun_slIsFeatureSupported>(interposer,"slIsFeatureSupported");
        auto setDevice=api<PFun_slSetD3DDevice>(interposer,"slSetD3DDevice");
        auto getFunction=api<PFun_slGetFeatureFunction>(interposer,"slGetFeatureFunction");
        const wchar_t* pluginPath=dir.c_str();sl::Feature features[]={sl::kFeatureDLSS_G,sl::kFeatureReflex};
        sl::Preferences pref{};pref.pathsToPlugins=&pluginPath;pref.numPathsToPlugins=1;
        pref.pathToLogsAndData=logDir.c_str();pref.featuresToLoad=features;pref.numFeaturesToLoad=2;
        pref.flags=sl::PreferenceFlags::eDisableCLStateTracking|sl::PreferenceFlags::eUseManualHooking;
        // Exact existing direct-x2 ProjectID identity; not an ApplicationID substitute.
        pref.engine=sl::EngineType::eCustom;pref.engineVersion="1.0.0";
        pref.projectId="3e6891d2-09ac-4f54-ae8d-f481c3150d4b";pref.renderAPI=sl::RenderAPI::eD3D12;
        std::cout<<"NGX_IDENTITY_API=PROJECT_ID\nNGX_PROJECT_ID="<<pref.projectId<<"\nNGX_ENGINE_VERSION="<<pref.engineVersion<<std::endl;
        stage="STREAMLINE_INIT";result("STREAMLINE_INIT_RESULT",init(pref,sl::kSDKVersion));initialized=true;
        // Init precedes device creation, as required by the public programming guide.
        stage="ADAPTER_SELECTION";ComPtr<IDXGIFactory1> factory;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter1> selected;DXGI_ADAPTER_DESC1 desc{};
        for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidate;auto code=factory->EnumAdapters1(i,&candidate);
            if(code==DXGI_ERROR_NOT_FOUND)break;hr(code);DXGI_ADAPTER_DESC1 d{};hr(candidate->GetDesc1(&d));
            if(d.VendorId==0x10de && !(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) && std::wstring(d.Description).find(L"RTX 3050 Ti")!=std::wstring::npos){selected=candidate;desc=d;break;}}
        if(!selected)throw std::runtime_error("Explicit NVIDIA RTX 3050 Ti adapter absent");
        std::wcout<<L"GPU_NAME="<<desc.Description<<std::endl;
        std::cout<<"VENDOR_ID="<<desc.VendorId<<"\nDEVICE_ID="<<desc.DeviceId<<"\nADAPTER_LUID=";
        auto luid=reinterpret_cast<const unsigned char*>(&desc.AdapterLuid);
        for(size_t i=0;i<sizeof(LUID);++i)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(luid[i]);
        std::cout<<std::dec<<std::endl;
        stage="D3D12_DEVICE";hr(D3D12CreateDevice(selected.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
        result("SET_D3D12_DEVICE_RESULT",setDevice(device.Get()));
        sl::AdapterInfo adapter{};adapter.deviceLUID=reinterpret_cast<uint8_t*>(&desc.AdapterLuid);adapter.deviceLUIDSizeInBytes=sizeof(LUID);
        stage="FEATURE_SUPPORT";result("DLSSG_FEATURE_SUPPORTED_RESULT",supported(sl::kFeatureDLSS_G,adapter));
        void *stateFn{},*optionsFn{};
        result("GET_STATE_FUNCTION_RESULT",getFunction(sl::kFeatureDLSS_G,"slDLSSGGetState",stateFn));
        result("SET_OPTIONS_FUNCTION_RESULT",getFunction(sl::kFeatureDLSS_G,"slDLSSGSetOptions",optionsFn));
        if(!stateFn||!optionsFn)throw std::runtime_error("Null public feature function");
        auto getState=reinterpret_cast<PFun_slDLSSGGetState*>(stateFn);
        auto setOptions=reinterpret_cast<PFun_slDLSSGSetOptions*>(optionsFn);
        sl::ViewportHandle viewport{0};sl::DLSSGState state{};
        stage="GET_STATE";result("DLSSG_GET_STATE_RESULT",getState(viewport,state,nullptr));
        std::cout<<"DLSSG_STATUS="<<unsigned(state.status)<<"\nNUM_FRAMES_TO_GENERATE_MAX="<<state.numFramesToGenerateMax
                 <<"\nREPORTED_VALUE_VIA_LOADED_SM86_PROXY="<<state.numFramesToGenerateMax<<std::endl;
        // This reports API acceptance only. Options take effect on a later Present;
        // this harness deliberately has no Present and does not claim generated frames.
        stage="SET_OPTIONS";
        for(uint32_t n=1;n<=4 && n<=state.numFramesToGenerateMax;++n) {
            sl::DLSSGOptions options{};options.mode=sl::DLSSGMode::eOn;options.numFramesToGenerate=n;
            auto key=std::string("SET_OPTIONS_")+std::to_string(n)+"_RESULT";
            result(key.c_str(),setOptions(viewport,options));
        }
        loadedModules();stage="SHUTDOWN";auto close=shutdown();initialized=false;result("STREAMLINE_SHUTDOWN_RESULT",close);
        std::cout<<"FAIL_STAGE=NONE\nFRAME_GENERATION_TESTED=NO"<<std::endl;
        // OS unload after shutdown and device release; no private cleanup or binary modifications.
        return 0;
    } catch(const std::exception& e) {
        std::cout<<"FAIL_STAGE="<<stage<<"\nERROR="<<e.what()<<"\nWIN32_ERROR="<<GetLastError()<<std::endl;
        loadedModules();
        if(initialized && shutdown){auto close=shutdown();std::cout<<"STREAMLINE_SHUTDOWN_RESULT="<<unsigned(close)<<std::endl;}
        return 1;
    }
}
