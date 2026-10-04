#pragma once
#include <d3d12sdklayers.h>
#include <dxgidebug.h>
#include <fstream>
#include <mutex>
#include <regex>
#include <vector>

// Diagnostic sinks only: no GPU-based validation, breaks, filters or render changes.
struct PresentDiagnostics {
    std::ofstream slLog, ngxLog, d3dLog, dxgiLog;
    std::mutex logMutex;
    ComPtr<ID3D12InfoQueue> d3d;
    ComPtr<IDXGIInfoQueue> dxgi;
    UINT64 d3dRead{}, dxgiRead{};
    fs::path backendLog;
    static PresentDiagnostics* active;
    static void callback(sl::LogType type, const char* message) {
        if (!active || !message) return;
        std::lock_guard<std::mutex> lock(active->logMutex);
        active->slLog << "tick=" << GetTickCount64() << " tid=" << GetCurrentThreadId()
                      << " type=" << unsigned(type) << ' ' << message << std::endl;
        if (strstr(message,"ngxLog") || strstr(message,"NGX") || strstr(message,"[DLSSG]"))
            active->ngxLog << "tick=" << GetTickCount64() << ' ' << message << std::endl;
    }
    void open(const fs::path& logs, const fs::path& runtime) {
        slLog.open(logs/"sl-callback.log"); ngxLog.open(logs/"ngx.log");
        d3dLog.open(logs/"d3d12-debug.log"); dxgiLog.open(logs/"dxgi-debug.log");
        active=this;
        wchar_t directory[32768]{};
        GetPrivateProfileStringW(L"Logging",L"Directory",L"",directory,32768,
                                (runtime/L"component/dlssg_sm86.ini").c_str());
        backendLog=fs::path(directory)/(L"backend_"+std::to_wstring(GetCurrentProcessId())+L".jsonl");
        ComPtr<ID3D12Debug> layer;
        HRESULT h=D3D12GetDebugInterface(IID_PPV_ARGS(&layer));
        if(SUCCEEDED(h))layer->EnableDebugLayer();
        std::cout << "DEBUG_LAYER_ENABLED=" << (SUCCEEDED(h)?"YES":"NO")
                  << "\nDEBUG_LAYER_HRESULT=0x" << std::hex << unsigned(h) << std::dec << std::endl;
        d3dLog << "DEBUG_LAYER_HRESULT=0x" << std::hex << unsigned(h) << std::dec << std::endl;
        HMODULE debug=LoadLibraryExW(L"dxgidebug.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using GetQueue=HRESULT(WINAPI*)(UINT,REFIID,void**);
        auto get=debug?reinterpret_cast<GetQueue>(GetProcAddress(debug,"DXGIGetDebugInterface1")):nullptr;
        h=get?get(0,IID_PPV_ARGS(&dxgi)):E_NOINTERFACE;
        std::cout << "DXGI_INFOQUEUE_ENABLED=" << (dxgi?"YES":"NO") << std::endl;
        dxgiLog << "DXGI_INFOQUEUE_HRESULT=0x" << std::hex << unsigned(h) << std::dec << std::endl;
    }
    void attach(ID3D12Device* device) {
        HRESULT h=device->QueryInterface(IID_PPV_ARGS(&d3d));
        std::cout << "D3D12_INFOQUEUE_ENABLED=" << (d3d?"YES":"NO") << std::endl;
        d3dLog << "D3D12_INFOQUEUE_HRESULT=0x" << std::hex << unsigned(h) << std::dec << std::endl;
    }
    void dump(const char* point) {
        d3dLog << "POINT=" << point << " tick=" << GetTickCount64() << std::endl;
        if(d3d) {
            const auto end=d3d->GetNumStoredMessagesAllowedByRetrievalFilter();
            for(;d3dRead<end;++d3dRead) {
                SIZE_T size{}; if(FAILED(d3d->GetMessage(d3dRead,nullptr,&size)))continue;
                std::vector<char> data(size); auto* m=reinterpret_cast<D3D12_MESSAGE*>(data.data());
                if(SUCCEEDED(d3d->GetMessage(d3dRead,m,&size)) && m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING)
                    d3dLog << "severity=" << unsigned(m->Severity) << " id=" << unsigned(m->ID)
                           << " description=" << m->pDescription << std::endl;
            }
        }
        dxgiLog << "POINT=" << point << " tick=" << GetTickCount64() << std::endl;
        if(dxgi) {
            const auto end=dxgi->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);
            for(;dxgiRead<end;++dxgiRead) {
                SIZE_T size{};if(FAILED(dxgi->GetMessage(DXGI_DEBUG_ALL,dxgiRead,nullptr,&size)))continue;
                std::vector<char> data(size);auto* m=reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(data.data());
                if(SUCCEEDED(dxgi->GetMessage(DXGI_DEBUG_ALL,dxgiRead,m,&size)) && m->Severity<=DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING)
                    dxgiLog << "severity=" << unsigned(m->Severity) << " id=" << unsigned(m->ID)
                            << " description=" << m->pDescription << std::endl;
            }
        }
    }
    bool evaluationSeen() {
        // Read existing text telemetry solely to bound this diagnostic run.
        // Counters in the final result come from the actual exit telemetry.
        std::ifstream input(backendLog);std::string line;
        static const std::regex event(R"re("event"\s*:\s*"evaluate(?:_[^"]*)?")re");
        static const std::regex counter(R"re("evaluates"\s*:\s*[1-9][0-9]*)re");
        while(std::getline(input,line))if(std::regex_search(line,event)||std::regex_search(line,counter))return true;
        return false;
    }
    ~PresentDiagnostics(){active=nullptr;}
};
PresentDiagnostics* PresentDiagnostics::active=nullptr;
