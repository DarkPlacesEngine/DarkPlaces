//current command in ascii decimal
let currentcmd = [0,0,0] 
let currentfile = "";
const sleep = ms => new Promise(r => setTimeout(r,ms));

let cmditerate = 0
Module['arguments'] = ["-basedir /save/data"]
Module['print'] = function(text){console.log(text);}
Module['preRun'] = function(){
    
    function stdin(){return 10};
    var stdout = null;
    var stderr = null; 
    FS.init(stdin,stdout,stderr);
    FS.mkdir('/save')
    FS.mount(IDBFS,{},"/save");
    
}
Module['noInitialRun'] = true
document.addEventListener('click', (ev) => {
    console.log("event is captured only once.");
    FS.syncfs(true);
    Module.callMain(["-basedir /save"]);
  }, { once: true });
  