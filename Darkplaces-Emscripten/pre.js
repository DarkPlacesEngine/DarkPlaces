Module['arguments'] = ['-basedir', '/quake', '-gamedir', 'id1'];

Module['print'] = function(text) {
	console.log(text);
}

Module['printErr'] = function(text) {
	console.error(text);
}

Module['preRun'] = [
	function()
	{
		function stdin(){
			return '\n';  // Return a newline/line feed character so the user is not prompted for input
		};
		FS.init(stdin, null, null); // null for both stdout and stderr
		FS.mkdir('/quake');
		FS.mount(IDBFS, {}, '/quake');
	}
]

Module['noInitialRun'] = true;

Module['onRuntimeInitialized'] = function() {
	FS.syncfs(true, function (err) {
		assert(!err);
		Module.callMain(Module.arguments);
	});
}
