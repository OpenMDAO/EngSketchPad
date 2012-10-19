/*
 *      wv: The Web Viewer
 *
 *              Render functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */


//
// A console function is added to the context: logger(msg).
// By default, it maps to the window.console() function on WebKit and to an 
// empty function on other browsers.
function logger(msg) 
{
    if (window.console && window.console.log) window.console.log(msg);
}


//
// A debug-based console function is added to the context: log(msg).
function log(msg) 
{
    if (g.debug != 0) logger(msg);
}


//
// checks for OpenGL errors
function checkGLError(gl, source) {
    if (g.debug <= 0) return;
    var error  = gl.getError();
    if (error != gl.NO_ERROR) {
        var str = "GL Error @ " + source + ": " + error;
        if (g.debug == 1) logger(str);
        if (g.debug >  1) throw str;
    }
}


//
// figures out what is running
var BrowserDetect = {
	init: function () {
		this.browser = this.searchString(this.dataBrowser) || "An unknown browser";
		this.version = this.searchVersion(navigator.userAgent)
			|| this.searchVersion(navigator.appVersion)
			|| "an unknown version";
		this.OS = this.searchString(this.dataOS) || "an unknown OS";
	},
	searchString: function (data) {
		for (var i=0;i<data.length;i++)	{
			var dataString = data[i].string;
			var dataProp = data[i].prop;
			this.versionSearchString = data[i].versionSearch || data[i].identity;
			if (dataString) {
				if (dataString.indexOf(data[i].subString) != -1)
					return data[i].identity;
			}
			else if (dataProp)
				return data[i].identity;
		}
	},
	searchVersion: function (dataString) {
		var index = dataString.indexOf(this.versionSearchString);
		if (index == -1) return;
		return parseFloat(dataString.substring(index+this.versionSearchString.length+1));
	},
	dataBrowser: [
		{
			string: navigator.userAgent,
			subString: "Chrome",
			identity: "Chrome"
		},
		{ 	string: navigator.userAgent,
			subString: "OmniWeb",
			versionSearch: "OmniWeb/",
			identity: "OmniWeb"
		},
		{
			string: navigator.vendor,
			subString: "Apple",
			identity: "Safari",
			versionSearch: "Version"
		},
		{
			prop: window.opera,
			identity: "Opera",
			versionSearch: "Version"
		},
		{
			string: navigator.vendor,
			subString: "iCab",
			identity: "iCab"
		},
		{
			string: navigator.vendor,
			subString: "KDE",
			identity: "Konqueror"
		},
		{
			string: navigator.userAgent,
			subString: "Firefox",
			identity: "Firefox"
		},
		{
			string: navigator.vendor,
			subString: "Camino",
			identity: "Camino"
		},
		{		// for newer Netscapes (6+)
			string: navigator.userAgent,
			subString: "Netscape",
			identity: "Netscape"
		},
		{
			string: navigator.userAgent,
			subString: "MSIE",
			identity: "Explorer",
			versionSearch: "MSIE"
		},
		{
			string: navigator.userAgent,
			subString: "Gecko",
			identity: "Mozilla",
			versionSearch: "rv"
		},
		{ 		// for older Netscapes (4-)
			string: navigator.userAgent,
			subString: "Mozilla",
			identity: "Netscape",
			versionSearch: "Mozilla"
		}
	],
	dataOS : [
		{
			string: navigator.platform,
			subString: "Win",
			identity: "Windows"
		},
		{
			string: navigator.platform,
			subString: "Mac",
			identity: "Mac"
		},
		{
			   string: navigator.userAgent,
			   subString: "iPhone",
			   identity: "iPhone/iPod"
	    },
		{
			string: navigator.platform,
			subString: "Linux",
			identity: "Linux"
		}
	]

}


//
// Initialize the Canvas element with the passed name as a WebGL object and 
// return the WebGLRenderingContext.
// Turn off anti-aliasing so that picking works at the fringe
function initWebGL(canvasName)
{
    var canvas = document.getElementById(canvasName);
    return  gl = WebGLUtils.setupWebGL(canvas, { antialias: false });
}


//
// Load this shader and return the WebGLShader object.
//
function loadShader(ctx, shaderID, shaderType, shaderSrc)
{

  // Create the shader object
  var shader = ctx.createShader(shaderType);

  // Load the shader source
  ctx.shaderSource(shader, shaderSrc);

  // Compile the shader
  ctx.compileShader(shader);

  // Check the compile status
  var compiled = ctx.getShaderParameter(shader, ctx.COMPILE_STATUS);
  if (!compiled && !ctx.isContextLost()) {
    // Something went wrong during compilation; get the error
    var error = ctx.getShaderInfoLog(shader);
    log("*** Error compiling shader "+shaderID+":"+error);
    ctx.deleteShader(shader);
    return null;
  }

  return shader;
}


//
// Load shaders with the passed sources and create a program with them. Return 
// this program in the 'program' property of the returned context.
//
// For each string in the passed attribs array, bind an attrib with that name 
// at that index. Once the attribs are bound, link the program and then use it.
//
// Set the clear color to the passed array (4 values) and set the clear depth 
// to the passed value.
// Enable depth testing
//
function wvSetup(gl, vshader, fshader, attribs, clearColor, clearDepth)
{
    // create our shaders
    var vertexShader   = loadShader(gl, "Vertex",    gl.VERTEX_SHADER,   vshader);
    var fragmentShader = loadShader(gl, "Fragement", gl.FRAGMENT_SHADER, fshader);

    // Create the program object
    var program = gl.createProgram();

    // Attach our two shaders to the program
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);

    // Bind attributes
    for (var i = 0; i < attribs.length; ++i)
        gl.bindAttribLocation(program, i, attribs[i]);

    // Link the program
    gl.linkProgram(program);

    // Check the link status
    var linked = gl.getProgramParameter(program, gl.LINK_STATUS);
    if (!linked && !gl.isContextLost()) {
        // something went wrong with the link
        var error = gl.getProgramInfoLog (program);
        log("Error in program linking:" + error);

        gl.deleteProgram(program);
        gl.deleteProgram(fragmentShader);
        gl.deleteProgram(vertexShader);

        return null;
    }

    gl.useProgram(program);

    gl.clearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    gl.clearDepth(clearDepth);

    gl.enable(gl.DEPTH_TEST);

    return program;
}


//
// initialize the Web Viewer
//
function wvInit()
{
  var requestId;
  
  // "globals" used:

  g.width    = -1;                                      // "canvas" size
  g.height   = -1;
  g.vbonum   =  0;                                      // vbo counter
  g.scale    =  1.0;                                    // global view scale
  g.picked   = undefined;                               // picked object
  g.sgUpdate = 0;                                       // sceneGraph update
  g.sceneUpd = 1;                                       // scene updated -- rerender
  if (g.debug     == undefined) g.debug     = 0;        // debug flag
  if (g.pick      == undefined) g.pick      = 0;        // picking flag (0-off, 1-on)
  if (g.eye       == undefined) g.eye       = [0.0, 0.0, 0.0];
  if (g.center    == undefined) g.center    = [0.0, 0.0, 0.0];
  if (g.up        == undefined) g.up        = [0,0, 1.0, 0.0];

  // define our plotting attributes
  g.plotAttrs  = { ON:1,          TRANSPARENT:2, SHADING:4,
                   ORIENTATION:8, POINTS:16,     LINES:32   };

  // initialize our scene graph
  g.sceneGraph = {};

  // initialize WebGL with the id of the Canvas Element
  var gl = initWebGL("WebViewer");
  if (!gl) return;

  //
  // the shaders
  var vShaderSrc = [
"    uniform mat4   u_modelViewMatrix;           // not currently used",
"    uniform mat4   u_modelViewProjMatrix;",
"    uniform mat4   u_normalMatrix;",
"    uniform vec3   lightDir;",
"    uniform vec3   conNormal;                   // constant normal",
"    uniform vec3   conColor;                    // constant color",
"    uniform vec3   bacColor;                    // back face color",
"    uniform float  wAmbient;                    // Ambient light weight",
"    uniform float  wColor;                      // Constant color switch",
"    uniform float  bColor;                      // Backface color switch",
"    uniform float  wNormal;                     // Constant normal switch",
"    uniform float  wLight;                      // lighting switch",
"    uniform float  xpar;                        // transparency factor",
"    uniform float  linAdj;                      // line Z adjustment",
"    uniform float  pointSize;                   // point size in pixels",
"    uniform int    picking;                     // picking flag",
"",
"    attribute vec3 vNormal;",
"    attribute vec4 vColor;",
"    attribute vec4 vPosition;",
"",
"    varying vec4   v_Color;",
"    varying vec4   v_bColor;",
"",
"    void main()",
"    {",
"        // set the pixel position",    
"        gl_Position     = u_modelViewProjMatrix * vPosition;",
"        gl_Position[2] += linAdj;",
"        if (picking != 0) return;",
"",
"        // assumes that colors are coming in as unsigned bytes",
"        vec4 color = vColor/255.0;",
"",
"        if (wLight <= 0.0) {",
"          // no lighting",
"          v_Color      = color*wColor + vec4(conColor,1)*(1.0-wColor);",
"          v_bColor     = v_Color;",
"          gl_PointSize = pointSize;            // set the point size",
"        } else {",
"          // setup bi-directional lighting",
"          //   a simple ambient/diffuse lighting model is used with:",
"          //      single 'white' source & no 'material' color",
"          //      linear mixture of ambient & diffuse based on weight",
"          vec3 lDirection = normalize(lightDir);",
"          vec3 norm       = vNormal*wNormal + conNormal*(1.0-wNormal);",
"          vec3 normal     = normalize(u_normalMatrix * vec4(norm,1)).xyz;",
"          float dot       = abs(dot(normal, lDirection));",
"",
"          // make the color to be rendered",
"          color           = color*wColor + vec4(conColor,1)*(1.0-wColor);",
"          v_Color         = color*dot + color*wAmbient;",
"          v_bColor        = v_Color;",
"          // are we coloring the backface?",
"          if (bColor != 0.0) {",
"              color       = vec4(bacColor,1);",
"              v_bColor    = color*dot + color*wAmbient;",
"          }",
"        }",
"        v_Color.a  = xpar;",
"        v_bColor.a = xpar;",
"    }"
  ].join("\n");

  var fShaderSrc = [
"    precision mediump float;",
"    uniform float bColor;                       // Backface color switch",
"    uniform int   picking;                      // picking flag",
"    uniform int   vbonum;                       // vbo number",
"",
"    varying vec4  v_Color;",
"    varying vec4  v_bColor;",
"",
"    void main()",
"    {",
"        if (picking == 0) {",
"          gl_FragColor = v_Color;",
"          if ((bColor != 0.0) && !gl_FrontFacing) gl_FragColor = v_bColor;",
"        } else {",
"          int high       = vbonum/256;",
"          gl_FragColor.r = float(high)/255.;",
"          gl_FragColor.g = float(vbonum - high*256)/255.;",
"          gl_FragColor.b = 0.0;",
"          gl_FragColor.a = 0.0;",
"//          high           = gl_PrimitiveID/256;",
"//          gl_FragColor.b = float(high)/255.;",
"//          gl_FragColor.a = float(gl_PrimitiveID - high*256)/255.;",
"        }",
"    }"
  ].join("\n");

  //
  // setup the shaders and other stuff for rendering
  g.program = wvSetup(gl,
                      // The sources of the vertex and fragment shaders.
                      vShaderSrc, fShaderSrc,
                      // The vertex attribute names used by the shaders.
                      // The order they appear here corresponds to their indices.
                      [ "vNormal", "vColor", "vPosition" ],
                      // The clear color and depth values.
                      [ 0.0, 0.0, 0.0, 0.0 ], 1.0);

  //
  // Set up the uniform variables for the shaders
  gl.uniform3f(gl.getUniformLocation(g.program,  "lightDir"), 0.0, 0.3, 1.0);
  gl.uniform1f(gl.getUniformLocation(g.program,  "wAmbient"), 0.25);

  g.u_xparLoc = gl.getUniformLocation(g.program,      "xpar");
  gl.uniform1f(g.u_xparLoc, 1.0);
  g.u_linAdjLoc = gl.getUniformLocation(g.program,    "linAdj");
  gl.uniform1f(g.u_linAdjLoc, 0.0);
  g.u_conNormalLoc = gl.getUniformLocation(g.program, "conNormal");
  gl.uniform3f(g.u_conNormalLoc, 0.0, 0.0, 1.0);
  g.u_conColorLoc = gl.getUniformLocation(g.program,  "conColor");
  gl.uniform3f(g.u_conColorLoc, 0.0, 1.0, 0.0);
  g.u_bacColorLoc = gl.getUniformLocation(g.program,  "bacColor");
  gl.uniform3f(g.u_bacColorLoc, 0.5, 0.5, 0.5);
  g.u_wColorLoc = gl.getUniformLocation(g.program,    "wColor");
  gl.uniform1f(g.u_wColorLoc, 1.0);
  g.u_bColorLoc = gl.getUniformLocation(g.program,    "bColor");
  gl.uniform1f(g.u_bColorLoc, 0.0);
  g.u_wNormalLoc = gl.getUniformLocation(g.program,   "wNormal");
  gl.uniform1f(g.u_wNormalLoc, 1.0);
   g.u_wLightLoc = gl.getUniformLocation(g.program,   "wLight");
  gl.uniform1f(g.u_wLightLoc, 1.0);
  g.u_pointSizeLoc = gl.getUniformLocation(g.program, "pointSize");
  gl.uniform1f(g.u_pointSizeLoc, 2.0);
  g.u_pickingLoc = gl.getUniformLocation(g.program,   "picking");
  gl.uniform1i(g.u_pickingLoc, 0);
  g.u_vbonumLoc = gl.getUniformLocation(g.program,    "vbonum");
  gl.uniform1i(g.u_vbonumLoc, 0);

  //
  // Create some matrices to use later and save their locations
  g.u_modelViewMatrixLoc =
                     gl.getUniformLocation(g.program, "u_modelViewMatrix");
  g.mvMatrix          = new J3DIMatrix4();
  g.mvMatrix.makeIdentity();
  g.u_normalMatrixLoc = gl.getUniformLocation(g.program, "u_normalMatrix");
  g.normalMatrix      = new J3DIMatrix4();
  g.u_modelViewProjMatrixLoc =
                 gl.getUniformLocation(g.program, "u_modelViewProjMatrix");
  g.mvpMatrix         = new J3DIMatrix4();
        
  return gl;
}


//
// startup function
//
function wvStart()
{
  var c = document.getElementById("WebViewer");
  c.addEventListener('webglcontextlost',     handleContextLost,     false);
  c.addEventListener('webglcontextrestored', handleContextRestored, false);
  
  BrowserDetect.init();
  logger(" Running: " + BrowserDetect.browser + " " + BrowserDetect.version +
               " on " + BrowserDetect.OS);

  //
  // init the web viewer
  var gl = wvInit();
  if (!gl) return;
  var nRbits = gl.getParameter(gl.RED_BITS);
  var nGbits = gl.getParameter(gl.GREEN_BITS);
  var nBbits = gl.getParameter(gl.BLUE_BITS);
  var nZbits = gl.getParameter(gl.DEPTH_BITS);
  logger(" WebGL Number of Bits: Red " +nRbits+"  Green "  +nGbits+
                              "  Blue "+nBbits+"  Zbuffer "+nZbits);
  g.lineBump = -0.0002;
  if (nZbits < 24) g.lineBump *= 2.0;
  
  //
  // initialize the UI
  wvInitUI();

  //
  // setup our render loop
  var f = function() {
  
    if (g.fov != undefined) drawPicture(gl);

    // update the UI and matrices
    wvUpdateUI();
    
    //update scene graph
    wvUpdateScene(gl);

    requestId = window.requestAnimFrame(f, c);
  };
  f();


  function handleContextLost(e) {
    e.preventDefault();
    if (requestId !== undefined) {
      window.cancelRequestAnimFrame(requestId);
      requestId = undefined;
    }
  }

  function handleContextRestored() {
    wvInit();
    f();
  }
}