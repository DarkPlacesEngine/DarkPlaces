//current command in ascii decimal
let currentcmd = [0,0,0] 
let currentfile = "";
const sleep = ms => new Promise(r => setTimeout(r,ms));
Module['print'] = function(text){console.log(text);}
Module['preRun'] = function(){
    
    function stdin(){return 10};
    var stdout = null;
    var stderr = null; 
    FS.init(stdin,stdout,stderr);
    FS.mount(IDBFS,{},"/home/web_user/");
    FS.symlink("/home/web_user","/save");
    
}
Module['noInitialRun'] = true
document.addEventListener('click', (ev) => {
    console.log("event is captured only once.");
    FS.syncfs(true,function(){Module.callMain(["-basedir","/home/web_user"]);});
    
  }, { once: true });

  