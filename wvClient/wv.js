// wv.js implements functions for wv
// written by John Dannenhoffer and Bob Haimes


function postMessage(mesg) {
    // alert("in postMessage(" + mesg + ")");

    var botm = document.getElementById("botmframe");

    var text = document.createTextNode(mesg);
    botm.insertBefore(text, botm.lastChild);

    var br = document.createElement("br");
    botm.insertBefore(br, botm.lastChild);

    br.scrollIntoView();
}


function resizeFrames() {

    // get the size of the client (minus 20 to account for possible scrollbars)
    var body = document.getElementById("mainBody");
    var bodyWidth  = body.clientWidth  - 20;
    var bodyHeight = body.clientHeight - 20;

    // get the elements associated with the frames and the canvas
    var left = document.getElementById("leftframe");
    var rite = document.getElementById("riteframe");
    var botm = document.getElementById("botmframe");
    var canv = document.getElementById("WebViewer");

    // compute and set the widths of the frames
    //    (do not make leftframe larger than 250px)
    var leftWidth = Math.round(0.25 * bodyWidth);
    if (leftWidth > 250)   leftWidth = 250;
    var riteWidth = bodyWidth - leftWidth;
    var canvWidth = riteWidth - 20;

    left.style.width = leftWidth + "px";
    rite.style.width = riteWidth + "px";
    botm.style.width = bodyWidth + "px";
    canv.style.width = canvWidth + "px";
    canv.width       = canvWidth;

    // compute and set the heights of the frames
    //    (do not make botm frame larger than 200px)
    var botmHeight = Math.round(0.20 * bodyHeight);
    if (botmHeight > 200)   botmHeight = 200;
    var  topHeight = bodyHeight - botmHeight;
    var canvHeight =  topHeight - 25;

    left.style.height =  topHeight + "px";
    rite.style.height =  topHeight + "px";
    botm.style.height = botmHeight + "px";
    canv.style.height = canvHeight + "px";
    canv.height       = canvHeight;
}

//
// Event Handlers
//

function getCursorXY(e)
{
    if (!e) var e = event;

    g.cursorX  = e.clientX;
    g.cursorY  = e.clientY;
    g.cursorX -= g.offLeft + 1;
    g.cursorY  = g.height - g.cursorY + g.offTop + 1;

    g.modifier = 0;
    if (e.shiftKey) g.modifier |= 1;
    if (e.altKey  ) g.modifier |= 2;
    if (e.ctrlKey ) g.modifier |= 4;
}


function getMouseDown(e)
{
    if (!e) var e = event;

    g.startX   = e.clientX;
    g.startY   = e.clientY;
    g.startX  -= g.offLeft + 1;
    g.startY   = g.height - g.startY + g.offTop + 1;

    g.dragging = true;
    g.button   = e.button;

    g.modifier = 0;
    if (e.shiftKey) g.modifier |= 1;
    if (e.altKey  ) g.modifier |= 2;
    if (e.ctrlKey ) g.modifier |= 4;
}


function getMouseUp(e)
{
    g.dragging = false;
}


function mouseLeftCanvas(e)
{
    if (g.dragging) {
        g.dragging = false;
    }
}


function getKeyPress(e)
{
    if (!e) var e = event;

    g.keyPress = e.charCode;
}

//
// Required WV functions
//

function wvInitUI()
{

    // set up extra storage for matrix-matrix multiplies
    g.uiMatrix = new J3DIMatrix4();

                                  // ui cursor variables
    g.cursorX  = -1;              // current cursor position
    g.cursorY  = -1;
    g.keyPress = -1;              // last key pressed
    g.startX   = -1;              // start of dragging position
    g.startY   = -1;
    g.button   = -1;              // button pressed
    g.modifier =  0;              // modifier (shift,alt,cntl) bitflag
    g.offTop   =  0;              // offset to upper-left corner of the canvas
    g.offLeft  =  0;
    g.dragging = false;           // true during drag operation
    g.picking  =  0;              // keycode of command that turns picking on
//  g.pick                        // set to 1 to turn picking on
//  g.picked                      // sceneGraph object that was picked
//  g.sceneUpd                    // set to 1 when scene needs rendering
//  g.sgUpdate                    // is 1 if the sceneGraph has been updated

    var canvas = document.getElementById("WebViewer");
      canvas.addEventListener('mousemove',  getCursorXY,     false);
      canvas.addEventListener('mousedown',  getMouseDown,    false);
      canvas.addEventListener('mouseup',    getMouseUp,      false);
      canvas.addEventListener('mouseout',   mouseLeftCanvas, false);
    document.addEventListener('keypress',   getKeyPress,     false);
}


function wvUpdateCanvas(gl)
{
}


function wvUpdateUI()
{

    // special code for delayed-picking mode
    if (g.picking > 0) {

        // if something is picked, post a message
        if (g.picked !== undefined) {

            // second part of 'q' operation
            if (g.picking == 113) {
                postMessage("Picked: " + g.picked.gprim);
            }

            g.picked  = undefined;
            g.picking = 0;
            g.pick    = 0;

        // abort picking on mouse motion
        } else if (g.dragging) {
            postMessage("Picking aborted");

            g.picking = 0;
            g.pick    = 0;
        }

        g.keyPress = -1;
        g.dragging = false;
    }

    // if the tree has not been created but the scene graph (possibly) exists...
    if (g.sgUpdate == 1 && (g.sceneGraph !== undefined)) {

        // ...count number of primitives in the scene graph
        var count = 0;
        for (var gprim in g.sceneGraph) {

            // parse the name
            var matches = gprim.split(" ");

            var ibody = Number(matches[1]);
            if        (matches[2] == "Face") {
                var iface = matches[3];
            } else if (matches[2] == "Loop") {
                var iloop = matches[3];
            } else if (matches[2] == "Edge") {
                var iedge = matches[3];
            } else {
                alert("unknown type: " + matches[2]);
                continue;
            }

            // determine if Body does not exists
            var knode = -1;
            for (var jnode = 1; jnode < myTree.name.length; jnode++) {
                if (myTree.name[jnode] == "Body " + ibody) {
                    knode = jnode;
                }
            }

            // if Body does not exist, create it and its Face, Loop, and Edge
            //    subnodes now
            var kface, kloop, kedge;
            if (knode < 0) {
                postMessage("Processing Body " + ibody);

                myTree.addNode(0, "Body " + ibody, "*");
                knode = myTree.name.length - 1;

                myTree.addNode(knode, "__Faces", "*");
                kface = myTree.name.length - 1;

                myTree.addNode(knode, "__Loops", "*");
                kloop = myTree.name.length - 1;

                myTree.addNode(knode, "__Edges", "*");
                kedge = myTree.name.length - 1;

            // otherwise, get pointers to the face-group and loop-group nodes
            } else {
                kface = myTree.child[knode];
                kloop = kface + 1;
                kedge = kloop + 1;
            }

            // make the tree node
            if        (matches[2] == "Face") {
                myTree.addNode(kface, "____face " + iface, gprim);
            } else if (matches[2] == "Loop") {
                myTree.addNode(kloop, "____loop " + iloop, gprim);
            } else if (matches[2] == "Edge") {
                myTree.addNode(kedge, "____edge " + iedge, gprim);
            }

            count++;
        }

        // if we had any primitives, we are assuming that we have all of
        //    them, so build the tree and remember that we have
        //    built the tree
        if (count > 0) {
            myTree.build();
            g.sgUpdate = 0;
        }
    }

    // deal with key presses
    if (g.keyPress != -1) {

        // '?' -- help
        if (g.keyPress ==  63) {
            postMessage("C - make tessellation coarser");
            postMessage("F - make tessellation finer");
            postMessage("q - query at cursor");
            postMessage("x - view from -X direction");
            postMessage("X - view from +X direction");
            postMessage("y - view from -Y direction");
            postMessage("Y - view from +Y direction");
            postMessage("z - view from -Z direction");
            postMessage("Z - view from +Z direction");
            postMessage("? - get help");

        // 'C' -- make tessellation coarser
        } else if (g.keyPress == 67) {
            postMessage("Retessellating...");
            g.socketUt.send("coarser");

        // 'F' -- make tessellation finer
        } else if (g.keyPress == 70) {
            postMessage("Retessellating...");
            g.socketUt.send("finer");

        // 'q' -- query at cursor
        } else if (g.keyPress == 113) {
            g.picking  = 113;
            g.pick     = 1;
            g.sceneUpd = 1;

        // 'x' -- view from -X direction
        } else if (g.keyPress == 120) {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(+90, 0,1,0);
            g.sceneUpd = 1;

        // 'X' -- view from +X direction
        } else if (g.keyPress ==  88) {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(-90, 0,1,0);
            g.sceneUpd = 1;

        // 'y' -- view from +Y direction
        } else if (g.keyPress == 121) {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(-90, 1,0,0);
            g.sceneUpd = 1;

        // 'Y' -- view from +Y direction
        } else if (g.keyPress ==  89) {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(+90, 1,0,0);
            g.sceneUpd = 1;

        // 'z' -- view from +Z direction
        } else if (g.keyPress ==  122) {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(180, 1,0,0);
            g.sceneUpd = 1;

        // 'Z' -- view from +Z direction
        } else if (g.keyPress ==  90) {
            g.mvMatrix.makeIdentity();
            g.sceneUpd = 1;

        } else {
            postMessage("'" + String.fromCharCode(g.keyPress)
                        + "' is not defined.  Use '?' for help.");
        }
    }

    g.keyPress = -1;

    // UI is in screen coordinates (not object)
    g.uiMatrix.load(g.mvMatrix);
    g.mvMatrix.makeIdentity();

    // deal with mouse movement
    if (g.dragging) {

        // cntrl is down (rotate)
        if (g.modifier == 4) {
            var angleX =  (g.startY - g.cursorY) / 4.0;
            var angleY = -(g.startX - g.cursorX) / 4.0;
            if ((angleX != 0.0) || (angleY != 0.0)) {
              g.mvMatrix.rotate(angleX, 1,0,0);
              g.mvMatrix.rotate(angleY, 0,1,0);
              g.sceneUpd = 1;
            }

        // alt-shift is down (rotate)
        } else if (g.modifier == 3) {
            var angleX =  (g.startY - g.cursorY) / 4.0;
            var angleY = -(g.startX - g.cursorX) / 4.0;
            if ((angleX != 0.0) || (angleY != 0.0)) {
              g.mvMatrix.rotate(angleX, 1,0,0);
              g.mvMatrix.rotate(angleY, 0,1,0);
              g.sceneUpd = 1;
            }

        // alt is down (spin)
        } else if (g.modifier == 2) {
            var xf = g.startX - g.width  / 2;
            var yf = g.startY - g.height / 2;

            if ((xf != 0.0) || (yf != 0.0)) {
                var theta1 = Math.atan2(yf, xf);
                xf = g.cursorX - g.width  / 2;
                yf = g.cursorY - g.height / 2;

                if ((xf != 0.0) || (yf != 0.0)) {
                    var dtheta = Math.atan2(yf, xf)-theta1;
                    if (Math.abs(dtheta) < 1.5708) {
                        var angleZ = 128*(dtheta)/3.1415926;
                        g.mvMatrix.rotate(angleZ, 0,0,1);
                        g.sceneUpd = 1;
                    }
                }
            }

        // shift is down (zoom)
        } else if (g.modifier == 1) {
            if (g.cursorY != g.startY) {
              var scale = Math.exp((g.cursorY - g.startY) / 512.0);
              g.mvMatrix.scale(scale, scale, scale);
              g.scale   *= scale;
              g.sceneUpd = 1;
            }

        // no modifier (translate)
        } else {
            var transX = (g.cursorX - g.startX) / 256.0;
            var transY = (g.cursorY - g.startY) / 256.0;
            if ((transX != 0.0) || (transY != 0.0)) {
              g.mvMatrix.translate(transX, transY, 0.0);
              g.sceneUpd = 1;
            }
        }

        g.startX = g.cursorX;
        g.startY = g.cursorY;
    }
}


function wvUpdateView()
{
    g.mvMatrix.multiply(g.uiMatrix);
}


function wvServerMessage(text)
{
    logger(" Server Message: " + text);
}

//
// needed when the canvas size changes or relocates
//

function reshape(gl)
{

    var canvas = document.getElementById('WebViewer');
    if (g.offTop != canvas.offsetTop || g.offLeft != canvas.offsetLeft) {
        g.offTop  = canvas.offsetTop;
        g.offLeft = canvas.offsetLeft;
    }

    if (g.width == canvas.width && g.height == canvas.height) return;

    g.width  = canvas.width;
    g.height = canvas.height;

    // Set the viewport and projection matrix for the scene
    gl.viewport(0, 0, g.width, g.height);
    g.perspectiveMatrix = new J3DIMatrix4();
    g.sceneUpd = 1;

    wvInitDraw();
}
