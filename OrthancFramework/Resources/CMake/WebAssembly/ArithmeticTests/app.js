function Initialize()
{
  Module.ccall('Run2', // name of C function
               null, // return type
               [], // argument types
               []);

  Module.ccall('Run', // name of C function
               'number', // return type
               [], // argument types
               []);
}


var Module = {
  preRun: [],
  postRun: [ Initialize ],
  print: function(text) {
    console.log(text);
  },
  printErr: function(text) {
    if (text != 'Calling stub instead of signal()')
    {
      document.getElementById("stderr").textContent += text + '\n';
    }
  },
  totalDependencies: 0
};


if (!('WebAssembly' in window)) {
  alert('Sorry, your browser does not support WebAssembly :(');
} else {
}
