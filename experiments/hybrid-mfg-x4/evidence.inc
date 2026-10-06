namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
constexpr uint32_t W=1280,H=720,DX=16;
static std::mutex logMutex;
static std::ofstream vendorLog,validationLog,validationMessagesLog;
static std::string jsonQuote(const std::string& s) {
 std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;
 else if(c=='\n')o<<"\\n";else if(c=='\r')o<<"\\r";else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c);else o<<c;}return o.str()+'"';
}
static std::string hexBytes(const uint8_t* p,size_t n){std::ostringstream o;o<<std::hex<<std::setfill('0');for(size_t i=0;i<n;++i)o<<std::setw(2)<<unsigned(p[i]);return o.str();}
static std::string resultHex(NVSDK_NGX_Result r){std::ostringstream o;o<<"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<uint32_t(r);return o.str();}
template<class T>static std::string handleText(T h){std::ostringstream o;o<<h;return o.str();}
static void NVSDK_CONV ngxLog(const char* s,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature){std::lock_guard l(logMutex);vendorLog<<s<<'\n';vendorLog.flush();}
static unsigned validationErrors=0;
static Bytes readFile(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read "+p.string());return Bytes(std::istreambuf_iterator<char>(f),{});}
static void save(const fs::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("Cannot write "+p.string());}
static std::string sha(const Bytes& b){BCRYPT_ALG_HANDLE a{};BCRYPT_HASH_HANDLE h{};DWORD n{},got{};
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA provider");
 BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&n),sizeof n,&got,0);Bytes obj(n),digest(32);
 if(BCryptCreateHash(a,&h,obj.data(),n,nullptr,0,0)<0||BCryptHashData(h,const_cast<PUCHAR>(b.data()),ULONG(b.size()),0)<0||BCryptFinishHash(h,digest.data(),32,0)<0)throw std::runtime_error("SHA operation");
 BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);return hexBytes(digest.data(),32);
}
struct EvidenceRecorder{
 fs::path out;std::map<std::string,std::string> data;std::ofstream events;std::ostringstream bootstrap;bool checkpointFailed=false;
 explicit EvidenceRecorder(fs::path p):out(p),events(p/"events.jsonl"){}
 void raw(std::string k,std::string v){data[k]=v;flush();}
 void str(std::string k,std::string v){raw(k,jsonQuote(v));}
 void event(std::string type,std::string detail){events<<"{\"timestamp_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<",\"pid\":"<<GetCurrentProcessId()<<",\"type\":"<<jsonQuote(type)<<",\"detail\":"<<jsonQuote(detail)<<"}\n";events.flush();}
 void flush(){
  auto serialize=[&](const fs::path& p){std::ofstream f(p);f<<'{';bool first=true;for(auto& [k,v]:data){f<<(first?"":",")<<jsonQuote(k)<<':'<<v;first=false;}f<<bootstrap.str()<<"}\n";f.flush();return bool(f);};
  auto tmp=out/"worker-result.tmp";DWORD error=ERROR_WRITE_FAULT;
  if(serialize(tmp))for(int attempt=0;attempt<5;++attempt){if(MoveFileExW(tmp.c_str(),(out/"worker-result.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return;error=GetLastError();if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_ACCESS_DENIED)break;Sleep(5);}
  // Evidence failures must not throw again while recording the original exception.
  checkpointFailed=true;data["evidence_checkpoint_failed"]="true";data["evidence_checkpoint_win32_error"]=std::to_string(error);
  std::ofstream(out/"evidence-io.log",std::ios::app)<<"checkpoint replacement failed Win32="<<error<<'\n';
  serialize(out/"worker-recovery.json");
 }
 void modules(std::string key){HMODULE ms[1024];DWORD n{};std::string list="[";if(EnumProcessModules(GetCurrentProcess(),ms,sizeof ms,&n)){for(DWORD i=0;i<std::min<DWORD>(n/sizeof(HMODULE),1024);++i){wchar_t p[32768];if(GetModuleFileNameExW(GetCurrentProcess(),ms[i],p,32768)){if(list.size()>1)list+=',';list+=jsonQuote(fs::path(p).string());}}}raw(key,list+"]");}
};
struct ExternalLoader{
 HMODULE module{};
 void start(EvidenceRecorder& r,const fs::path& dll,unsigned requestedMax=1){
  if(requestedMax<1||requestedMax>4)throw std::runtime_error("Invalid phase multiplier request");
  r.raw("external_requested_max",std::to_string(requestedMax));
  r.str("if_fail_stage","PRECONDITION");auto hash=sha(readFile(dll));r.str("external_dll_sha256",hash);r.str("external_dll_path",dll.string());
  if(hash!="c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838")throw std::runtime_error("Not the identified sdli 0.3.5 module");
  r.modules("modules_before_external");std::ofstream ini(dll.parent_path()/"dlssg_sm86.ini");
  ini<<"[General]\nEnabled=1\n[FrameGeneration]\nOptimized=0\nMaxGeneratedFrames="<<requestedMax<<"\n[Compatibility]\nRouter=SM86\nKernelImage=Auto\nSpoofArchToGame=0\n[Logging]\nLevel=3\nDirectory="<<(r.out/"backend").string()<<"\n[Runtime]\nMode=Bundled\nCacheDirectory="<<(r.out/"bundle-cache").string()<<'\n';ini.close();if(!ini)throw std::runtime_error("INI write failed");
  r.str("if_fail_stage","LOADER");r.event("external_load_begin",dll.string());module=LoadLibraryW(dll.c_str());auto err=GetLastError();r.str("external_module",handleText(module));r.raw("loader_win32_error",module?"0":std::to_string(err));if(!module)throw std::runtime_error("External LoadLibrary failed Win32="+std::to_string(err));
  r.str("public_initializer","NONE_SDLI_LOAD_CONTRACT");r.str("external_loader_ready","LOADED");r.str("backend_install_observation","NOT_YET_OBSERVED");r.modules("modules_after_external");r.event("external_load_complete",handleText(module));
 }
 // No destructor: hooks and module remain resident until worker process exit.
};
