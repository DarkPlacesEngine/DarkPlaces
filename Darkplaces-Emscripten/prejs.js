Module['print'] = function(text){console.log(text);}
//and this runs right after calling main but before the C code actually runs
Module['preRun'].push(function()
{
    
    function stdin(){return 10};
    var stdout = null;
    var stderr = null; 
    FS.init(stdin,stdout,stderr);
    FS.mount(IDBFS,{},"/home/web_user/");
    FS.symlink("/home/web_user","/save");
    
})
Module['noInitialRun'] = true
//The function is defined here because Module.onRuntimeInitialized runs after loading everything so this
//ensures the end user can't start the game before files load
Module['onRuntimeInitialized'] = function(){
    //Event listener is required as typical web browsers won't let you load IndexedDB before the user
    //interacts with the page. Instead, it would just crash.
    document.addEventListener('click', (ev) => {
        console.log("event is captured only once.");
        args = ["-basedir"]
        if(window.location.href.indexOf("file://") > -1)
        {
            args.push("/home/web_user")
            try 
            {
                args = args.concat(prompt("Enter command line arguments").split(" "))
            } catch (error) 
            {
                console.log("Error: ",error);
                console.log("Failed to concat extra arguments (likely passed nothing for the argument)")
            }
            

        } else
        {
            args.push("/game")
            parms = new URLSearchParams(window.location.search);
            try 
            {
                args = args.concat(parms.get("args").split(" "))
            } catch (error) 
            {
                console.log("Error: ",error);
                console.log("Failed to concat extra arguments (likely passed nothing for the argument)")
            }
            
        }
        FS.syncfs(true,function(){Module.callMain(args);});
        
    }, { once: true });
}