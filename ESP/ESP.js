// ESP.js implements functions for the Engineering Sketch Pad (ESP)
// written by John Dannenhoffer and Bob Haimes

// functions expected by wv
//    wvInitUI()
//    wvUpdateCanvas(gl)
//    wvUpdateUI()
//    wvUpdateView()
//    wvServerMessage(text)

// functions associated with button presses
//    activateBuildButton()
//    cmdUndo()
//    cmdSave()
//    cmdHelp()

// functions associated with mouse clicks and context-menu selections
//    showLinkages()
//    clearLinkages()
//    addPmtr()
//    editPmtr(e)
//    addBrch(e)
//    delBrch(e)
//    editBrch(e)
//    addAttr(e)
//    editAttr(e)
//    buildTo(e)
//    rememberCmenuId(e)

// functions associated with the mouse in the canvas
//    getCursorXY(e)
//    getMouseDown(e)
//    getMouseUp(e)
//    mouseLeftCancas(e)
//    getKeyPress(e)

// functions associated with toggling display settings
//    toggleViz(e)
//    toggleGrd(e)
//    toggleTrn(e)
//    toggleOri(e)

// functions associated with a Tree in the leftframe
//    Tree(doc, treeId) - constructor
//    TreeAddNode(iparent, name, gprim, click, cmenu, prop1, valu1, cbck1, prop2, valu2, cbck2, prop3, valu3, cbck3)
//    TreeBuild()
//    TreeClear()
//    TreeContract(inode)
//    TreeExpand(inode)
//    TreeProp(inode, iprop, onoff)
//    TreeUpdate()

// helper functions
//    postMessage(mesg)
//    resizeFrames()
//    reshape(gl)
//    buildInputsObject(type)

"use strict";


//
// called when the user interface is to be initialized (called by wv-render.js)
//
function wvInitUI()
{
    // alert("wvInitUI()");

    // set up extra storage for matrix-matrix multiplies
    g.uiMatrix   = new J3DIMatrix4();
    g.saveMatrix = new J3DIMatrix4(g.mvMatrix);

                                   // ui cursor variables
    g.cursorX   = -1;              // current cursor position
    g.cursorY   = -1;
    g.keyPress  = -1;              // last key pressed
    g.startX    = -1;              // start of dragging position
    g.startY    = -1;
    g.button    = -1;              // button pressed
    g.modifier  =  0;              // modifier (shift,alt,cntl) bitflag
    g.flying    =  1;              // flying multiplier (do not set to 0)
    g.offTop    =  0;              // offset to upper-left corner of the canvas
    g.offLeft   =  0;
    g.dragging  =  false;          // true during drag operation
    g.picking   =  0;              // keycode of command that turns picking on
    g.pmtrStat  =  0;              // -2 latest Parameters are in Tree
                                   // -1 latest Parameters not in Tree (yet)
                                   //  0 need to request Parameters
                                   // >0 waiting for Parameters (request already made)
    g.brchStat  =  0;              // -2 latest Branches are in Tree
                                   // -1 latest Branches not in Tree (yet)
                                   //  0 need to request Branches
                                   // >0 waiting for Branches (request already made)
    g.cmenuId   = undefined;       // ID of TD on which cmenu was fired
    g.server    = undefined;       // string passed back from "identify;"
//  g.pick                         // set to 1 to turn picking on
//  g.picked                       // sceneGraph object that was picked
//  g.sceneUpd                     // should be set to 1 to re-render scene
//  g.sgUpdate                     // =1 if the sceneGraph has been updated

    var canvas = document.getElementById("WebViewer");
      canvas.addEventListener('mousemove',  getCursorXY,     false);
      canvas.addEventListener('mousedown',  getMouseDown,    false);
      canvas.addEventListener('mouseup',    getMouseUp,      false);
      canvas.addEventListener('mouseout',   mouseLeftCanvas, false);
    document.addEventListener('keypress',   getKeyPress,     false);
}


//
// called when the canvas should be updated (called by wv-draw.js)
//
function wvUpdateCanvas(gl)
{
    // alert("wvUpdateConvas("+gl+")");
}


//
// called when the user interface should be updated (called by wv-render.js)
//
function wvUpdateUI()
{
    // alert("wvUpdateUI()");

    // special code for delayed-picking mode
    if (g.picking > 0) {

        // if something is picked, post a message
        if (g.picked !== undefined) {

            // second part of 'q' operation
            if        (g.picking == 113) {
                postMessage("Picked: "+g.picked.gprim);

                try {
                    var attrs = sgData[g.picked.gprim];
                    for (var i = 0; i < attrs.length; i+=2) {
                        postMessage(". . . . "+attrs[i]+"= "+attrs[i+1]);
                    }
                } catch (x) {
                }
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

    // if the Parameters are scheduled to be updated, send a message to
    //    get the Parameters now
    if (g.pmtrStats > 0) {
        g.pmtrStat--;
    } else if (g.pmtrStat == 0) {
        try {
            g.socketUt.send("identify;");
            g.socketUt.send("getPmtrs;");
            g.pmtrStat = 60;
        } catch (e) {
            // could not send command
        }
    }

    // if the Branches are scheduled to be updated, send a message to
    //    get the Branches now
    if (g.brchStats > 0) {
        g.brchStat--;
    } else if (g.brchStat == 0) {
        try {
            g.socketUt.send("getBrchs;");
            g.brchStat = 60;
        } catch (e) {
            // could not send command
        }
    }

    // if the scene graph and Parameters have been updated, (re-)build the Tree
    if ((g.sgUpdate == 1 && g.pmtrStat <= -1 && g.brchStat <= -1) ||
        (                   g.pmtrStat == -1 && g.brchStat == -2) ||
        (                   g.pmtrStat == -2 && g.brchStat == -1)   ) {

        if (g.sceneGraph === undefined) {
            alert("g.sceneGraph is undefined --- but we need it");
        }

        // if there was a previous Tree, keep track of whether or not
        //    the Parameters, Branches, and Display was open
        var pmtrsOpen = 0;
        var brchsOpen = 0;

        if (myTree.opened.length > 3) {
            pmtrsOpen = myTree.opened[1];
            brchsOpen = myTree.opened[2];
        }

        // clear previous Nodes from the Tree
        myTree.clear();

        // put the group headers into the Tree
        myTree.addNode(0, "Parameters", "", addPmtr, "pmtr1_cmenu");
        myTree.addNode(0, "Branches",   "", addBrch, "brch1_cmenu");
        myTree.addNode(0, "Display",    "", null,    ""           );

        // put the Parameters into the Tree
        for (var ipmtr = 0; ipmtr < pmtr.length; ipmtr++) {
            var name  = "__"+pmtr[ipmtr].name;
            var type  =      pmtr[ipmtr].type;
            var nrow  =      pmtr[ipmtr].nrow;
            var ncol  =      pmtr[ipmtr].ncol;
            var value =      pmtr[ipmtr].value[0];

            if (nrow > 1 || ncol > 1) {
                value = "["+nrow+"x"+ncol+"]";
            }

            if (type == 500) {       // OCSM_EXTERNAL
                myTree.addNode(1, name, "", editPmtr, "pmtr2_cmenu", ""+value, "", "");
            } else {
                myTree.addNode(1, name, "", null,     "",            ""+value, "", "");
            }
        }

        g.pmtrStat = -2;

        // open the Parameters (if they were open before the Tree was rebuilt)
        if (pmtrsOpen == 1) {
            myTree.opened[1] = 1;
        }

        // put the Branches into the Tree
        for (var ibrch = 0; ibrch < brch.length; ibrch++) {
            var name  = "__"+brch[ibrch].name;
            var type  =      brch[ibrch].type;
            var actv;
            if        (brch[ibrch].actv == 301) {
                actv = "suppressed";
            } else if (brch[ibrch].actv == 302) {
                actv = "inactive";
            } else if (brch[ibrch].actv == 303) {
                actv = "deferred";
            } else {
                actv = "";
            }

            myTree.addNode(2, name, "", editBrch, "brch2_cmenu", type, "", "", actv, "", "");

            // add the Branch's attributes to the Tree
            var inode = myTree.name.length - 1;
            for (var iattr = 0; iattr < brch[ibrch].attrs.length; iattr++) {
                var aname  = brch[ibrch].attrs[iattr][0];
                var avalue = brch[ibrch].attrs[iattr][1];
                myTree.addNode(inode, "____"+aname, "", editAttr, "attr2_cmenu", avalue, "", "");
            }
        }

        g.brchStat = -2;

        // open the Branches (if they were open before the Tree was rebuilt)
        if (brchsOpen == 1) {
            myTree.opened[2] = 1;
        }

        // put the Display attributes into the Tree
        for (var gprim in g.sceneGraph) {

            // parse the name
            var matches = gprim.split(" ");

            var ibody = Number(matches[1]);
            if        (matches[2] == "Face") {
                var iface = matches[3];
            } else if (matches[2] == "Edge") {
                var iedge = matches[3];
            } else {
                alert("unknown type: "+matches[2]);
                continue;
            }

            // determine if Body does not exists
            var kbody = -1;
            for (var jnode = 1; jnode < myTree.name.length; jnode++) {
                if (myTree.name[jnode] == "__Body "+ibody) {
                    kbody = jnode;
                }
            }

            // if Body does not exist, create it and its Face and Edge
            //    subnodes now
            var kface, kedge;
            if (kbody < 0) {
                myTree.addNode(3, "__Body "+ibody, "", null, "",
                               "Viz", "on", toggleViz, "Grd", "off", toggleGrd);
                kbody = myTree.name.length - 1;

                myTree.addNode(kbody, "____Faces", "", null, "",
                               "Viz", "on", toggleViz, "Grd", "off", toggleGrd, "Trn", "off", toggleTrn);
                kface = myTree.name.length - 1;

                myTree.addNode(kbody, "____Edges", "", null, "",
                               "Viz", "on", toggleViz, "Grd", "off", toggleGrd, "Ori", "off", toggleOri);
                kedge = myTree.name.length - 1;

            // otherwise, get pointers to the face-group and edge-group Nodes
            } else {
                kface = myTree.child[kbody];
                kedge = kface + 1;
            }

            // make the Tree Node
            if        (matches[2] == "Face") {
                myTree.addNode(kface, "______face "+iface, gprim, null, "disp2_cmenu",
                               "Viz", "on", toggleViz, "Grd", "off", toggleGrd, "Trn", "off", toggleTrn);
            } else if (matches[2] == "Edge") {
                myTree.addNode(kedge, "______edge "+iedge, gprim, null, "disp2_cmenu",
                               "Viz", "on", toggleViz, "Grd", "off", toggleGrd, "Ori", "off", toggleOri);
            }
        }

        // open the Branches (by default)
        myTree.opened[3] = 1;

        // mark that we have (re-)built the Tree
        g.sgUpdate = 0;

        // convert the abstract Tree Nodes into an HTML table
        myTree.build();
    }

    // deal with key presses
    if (g.keyPress != -1) {

        var myKeyPress = String.fromCharCode(g.keyPress);

        // '?' -- help
        if (myKeyPress == "?") {
            postMessage(".......... Viewer Cursor options ..........");
            postMessage("q - query at cursor. . . . . . . . . . . . ");
            postMessage("x - view from -X . . . . . X - view from +X");
            postMessage("y - view from -Y . . . . . Y - view from +Y");
            postMessage("Y - view from +Y . . . . . Z - view from +Z");
            postMessage("> - save view. . . . . . . < - recall view ");
            postMessage("! - toggle flying mode . . ? - get help    ");

        // 'q' -- query at cursor
        } else if (myKeyPress == "q") {
            g.picking  = 113;
            g.pick     = 1;
            g.sceneUpd = 1;

        // 'x' -- view from -X direction
        } else if (myKeyPress == "x") {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(+90, 0,1,0);
            g.sceneUpd = 1;

        // 'X' -- view from +X direction
        } else if (myKeyPress == "X") {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(-90, 0,1,0);
            g.sceneUpd = 1;

        // 'y' -- view from +Y direction
        } else if (myKeyPress == "y") {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(-90, 1,0,0);
            g.sceneUpd = 1;

        // 'Y' -- view from +Y direction
        } else if (myKeyPress == "Y") {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(+90, 1,0,0);
            g.sceneUpd = 1;

        // 'z' -- view from +Z direction
        } else if (myKeyPress == "z") {
            g.mvMatrix.makeIdentity();
            g.mvMatrix.rotate(180, 1,0,0);
            g.sceneUpd = 1;

        // 'Z' -- view from +Z direction
        } else if (myKeyPress == "Z") {
            g.mvMatrix.makeIdentity();
            g.sceneUpd = 1;

        // '!' -- toggle flying mode
        } else if (myKeyPress == "!") {
            if (g.flying <= 1) {
                postMessage("turning flying mode ON");
                g.flying = 10;
            } else {
                postMessage("turning flying mode OFF");
                g.flying = 1;
            }

        // '>' -- save view
        } else if (myKeyPress == ">") {
            postMessage("Saving current view");
            g.saveMatrix.load(g.mvMatrix);
            g.sceneUpd = 1;

        // '<' -- recall view
        } else if (myKeyPress == "<") {
            g.mvMatrix.load(g.saveMatrix);
            g.sceneUpd = 1;

        // unknown command
        } else if (g.keyPress == 0) {
            postMessage("'<ESC>' is not defined.  Use '?' for help.");
        } else {
            postMessage("'"+myKeyPress+"' ("+g.keyPress+") is not defined.  Use '?' for help.");
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
            var angleX =  (g.startY - g.cursorY) / 4.0 / g.flying;
            var angleY = -(g.startX - g.cursorX) / 4.0 / g.flying;
            if ((angleX != 0.0) || (angleY != 0.0)) {
                g.mvMatrix.rotate(angleX, 1,0,0);
                g.mvMatrix.rotate(angleY, 0,1,0);
                g.sceneUpd = 1;
            }

        // alt-shift is down (rotate)
        } else if (g.modifier == 3) {
            var angleX =  (g.startY - g.cursorY) / 4.0 / g.flying;
            var angleY = -(g.startX - g.cursorX) / 4.0 / g.flying;
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
                    var dtheta = Math.atan2(yf, xf) - theta1;
                    if (Math.abs(dtheta) < 1.5708) {
                        var angleZ = 128*(dtheta) / 3.1415926 / g.flying;
                        g.mvMatrix.rotate(angleZ, 0,0,1);
                        g.sceneUpd = 1;
                    }
                }
            }

        // shift is down (zoom)
        } else if (g.modifier == 1) {
            if (g.cursorY != g.startY) {
                var scale = Math.exp((g.cursorY - g.startY) / 512.0 / g.flying);
                g.mvMatrix.scale(scale, scale, scale);
                g.scale   *= scale;
                g.sceneUpd = 1;
            }

        // no modifier (translate)
        } else {
            var transX = (g.cursorX - g.startX) / 256.0 / g.flying;
            var transY = (g.cursorY - g.startY) / 256.0 / g.flying;
            if ((transX != 0.0) || (transY != 0.0)) {
                g.mvMatrix.translate(transX, transY, 0.0);
                g.sceneUpd = 1;
            }
        }

        // if not flying, then update the start coordinates
        if (g.flying <= 1) {
            g.startX = g.cursorX;
            g.startY = g.cursorY;
        }
    }
}


//
// called when the view is updated (called by wv-draw.js)
//
function wvUpdateView()
{
    // alert("wvUpdateView)");

    g.mvMatrix.multiply(g.uiMatrix);
}


//
// called when a (text) message is received from the server (called by wv-socket.js)
//
function wvServerMessage(text)
{
    // alert("wvServerMessage("+text+")");

    // if it starts with "identify;" post a message */
    if        (text.substring(0,9) == "identify;") {
        if (g.server === undefined) {
            g.server = text.substring(9,text.length-2);
            postMessage("ESP has been initialized and is attached to '"+g.server+"'");
        }

    // if it starts with "sgData;" store the auxiliary scene graph data
    } else if (text.substring(0,7) == "sgData;") {
        if (text.length > 9) {
            sgData = JSON.parse(text.substring(7,text.length-1));
        } else {
            sgData = {};
        }

    // if it starts with "getPmtrs;" build the (global) pmtr array
    } else if (text.substring(0,9) == "getPmtrs;") {
        if (text.length > 11) {
            pmtr = JSON.parse(text.substring(9,text.length-1));
        } else {
            pmtr = new Array;
        }
        g.pmtrStat = -1;

    // if it starts with "newPmtr;" do nothing
    } else if (text.substring(0,8) == "newPmtr;") {

    // if it starts with "setPmtr;" do nothing
    } else if (text.substring(0,8) == "setPmtr;") {

    // if it starts with "getBrchs;" build the (global) brch array
    } else if (text.substring(0,9) == "getBrchs;") {
        if (text.length > 11) {
            brch = JSON.parse(text.substring(9,text.length-1));
        } else {
            brch = new Array;
        }
        g.brchStat = -1;

    // if it starts with "newBrch;" do nothing
    } else if (text.substring(0,8) == "newBrch;") {

    // if it starts with "setBrch;" do nothing
    } else if (text.substring(0,8) == "setBrch;") {

    // if it starts with "delBrch;" do nothing
    } else if (text.substring(0,8) == "delBrch;") {

    // if it starts with "setAttr;" do nothing
    } else if (text.substring(0,8) == "setAttr;") {

    // if it starts with "undo;" do nothing
    } else if (text.substring(0,5) == "undo;") {
        var cmd = text.substring(5,text.length-2);
        postMessage("Undoing \""+cmd+"\" ====> Re-build is needed <====");

    // if it starts with "save;" do nothing
    } else if (text.substring(0,5) == "save;") {

    // if it starts with "build;" reset the build button
    } else if (text.substring(0,6) == "build;") {
        var button = document.getElementById("buildButton");
        button.onclick = null;
        button["innerHTML"] = "Up to date";
        button.style.backgroundColor = null;

        var textList = text.split(";");
        var ibrch    = textList[1];
        var nbody    = textList[2];

        if (ibrch == brch.length) {
            postMessage("Entire build complete, which generated "+nbody+" Body(s)");
        } else {
            postMessage("Partial build (through "+brch[ibrch-1].name+") complete, which generated "+nbody+" Body(s)");
        }
        postMessage(" ");

    // if it starts with "ERROR:: this is nothing to undo" post the error
    } else if (text.substring(0,32) == "ERROR:: there is nothing to undo") {
        postMessage(text);
        alert("There is nothing to undo");
        
    // if it starts with "ERROR::" post the error
    } else if (text.substring(0,7) == "ERROR::") {
        postMessage(text);
        alert("Error encountered (see messages)");

        var button = document.getElementById("buildButton");

        button["innerHTML"] = "Fix before re-build";
        button.style.backgroundColor = "#FF3F3F";

        button.onclick = function () {
        };

    // default is to post the message
    } else {
        postMessage(text);
    }
}


//
// activate the buildButton
//
function activateBuildButton () {
    // alert("in activateBuildButton()");

    var button = document.getElementById("buildButton");

    button["innerHTML"] = "Press to Re-build";
    button.style.backgroundColor = "#3FFF3F";

    button.onclick = function () {
        postMessage("Re-building...");

        g.socketUt.send("getPmtrs;");
        g.pmtrStat = 60;

        g.socketUt.send("getBrchs;");
        g.brchStat = 60;

        g.socketUt.send("build;0;");

        var button = document.getElementById("buildButton");
        button.onclick = null;
        button["innerHTML"] = "Re-bulding...";
        button.style.backgroundColor = "#FFFF3F";
    };
}


//
// called when "undoButton" is pressed (defined in ESP.html)
//
function cmdUndo () {
    // alert("in cmdUndo()");

    if (g.server != "serveCSM") {
        alert("cmdUndo is not implemented for "+g.server);
        return;
    }

    g.socketUt.send("undo;");

    // update the UI
    activateBuildButton();
}


//
// called when "saveButton" is pressed (defined in ESP.html)
//
function cmdSave () {
    // alert("in cmdSave()");

    if (g.server != "serveCSM") {
        alert("cmdSave is not implemented for "+g.server);
        return;
    }

    var filename = prompt("Enter filename:");
    if (filename !== null) {
        if (filename.search(".csm") == -1) {
            filename = filename + ".csm";
        }

        postMessage("Saving model to '"+filename+"'");
        g.socketUt.send("save;"+filename+";");
    } else {
        postMessage("NOT saving since no filename specified");
    }
}


//
// called when "helpButton" is pressed (defined in ESP.html)
//
function cmdHelp () {

    // open help in another tab
    window.open("ESP-help.html");
}


//
// called when "Show linkages" is selected in pmtr2_cmenu (defined in ESP.html)
// called when "Show linkages" is selected in brch2_cmenu (defined in ESP.html)
// called when "Show linkages" is selected in disp2_cmenu (defined in ESP.html)
//
function showLinkages() {
    alert("showLinkages("+g.cmenuId+") called");

    g.cmenuId = undefined;
}


//
// called when "Clear linkages" is selected in pmtr2_cmenu (defined in ESP.html)
// called when "Clear linkages" is selected in brch2_cmenu (defined in ESP.html)
// called when "Clear linkages" is selected in disp2_cmenu (defined in ESP.html)
//
function clearLinkages() {
    alert("clearLinkages() called");

    g.cmenuId = undefined;
}


//
// called when "Add a Parameter" is selected in pmtr1_cmenu (defined in ESP.html)
//
function addPmtr() {
    // alert("addPmtr() called");

    if (g.server != "serveCSM") {
        alert("addPmtr is not implemented for "+g.server);
        return;
    }

    g.cmenuId = undefined;

    // get the new name
    var name = prompt("Enter new Parameter name");
    if (name === null) {
        return;
    } else if (name.length <= 0) {
        return;
    }

    // check that name is valid
    if (name.match(/^[a-zA-Z]\w*$/) === null) {
        alert("'"+name+"' is not a valid name");
        return;
    }

    // check that the name does not exist already
    for (var ipmtr = 0; ipmtr < pmtr.length; ipmtr++) {
        if (name == pmtr[ipmtr].name) {
            alert("'"+name+"' already exists");
            return;
        }
    }

    // get the number of rows
    var nrow = prompt("Enter number of rows");
    if (nrow !== null) {
        if (isNaN(nrow)) {
            alert("Number of rows must be positive integer");
            return;
        } else if (Number(nrow) < 1) {
            alert("Number of rows must be positive integer");
            return;
        } else if (Math.round(Number(nrow)) != Number(nrow)) {
            alert("Number of rows must be positive integer");
            return;
        }
    } else {
        return;
    }

    // get the number of columns
    var ncol = prompt("Enter number of columns");
    if (ncol !== null) {
        if (isNaN(ncol)) {
            alert("Number of columns must be positive integer");
            return;
        } else if (Number(ncol) < 1) {
            alert("Number of columns must be positive integer");
            return;
        } else if (Math.round(Number(ncol)) != Number(ncol)) {
            alert("Number of columns must be positive integer");
            return;
        }
    } else {
        return;
    }

    // get each of the values
    var value = new Array();
    var index = -1;
    for (var irow = 1; irow <= nrow; irow++) {
        for (var icol = 1; icol <= ncol; icol++) {
            index++;

            // get the new value
            if (nrow == 1 && ncol == 1) {
                value[index] = prompt("Enter new value for "+name, "");
            } else {
                value[index] = prompt("Enter new value for "+name+"["+irow+","+icol+"]", "");
            }

            // make sure a valid number was entered
            if (value[index] === null) {
                alert("No entry made");
                return;
            } else if (isNaN(value[index])) {
                alert("Illegal number format");
                return;
            }
        }
    }

    // store the values locally
    ipmtr       = pmtr.length;
    pmtr[ipmtr] = {};

    pmtr[ipmtr].name  = name;
    pmtr[ipmtr].type  = 500;
    pmtr[ipmtr].nrow  = nrow;
    pmtr[ipmtr].ncol  = ncol;
    pmtr[ipmtr].value = value;

    // set the new Parameter to the server
    var mesg = "newPmtr;"+name+";"+nrow+";"+ncol+";";

    index = -1;
    for (var irow = 1; irow <= nrow; irow++) {
        for (var icol = 1; icol <= ncol; icol++) {
            index++;
            mesg = mesg+value[index]+";";
        }
    }

    g.socketUt.send(mesg);

    // update the UI
    postMessage("Parameter '"+name+"' has been added ====> Re-build is needed <====");
    activateBuildButton();
}


//
// called when "Edit" is selected in pmtr2_cmenu (defined in ESP.html)
//
function editPmtr(e) {
    // alert("in editPmtr("+e+")");

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    // get the Tree Node
    var inode = Number(id.substring(4,id.length-4));

    // get the Parameter name
    var name = myTree.name[inode];
    while (name.charAt(0) == "_") {
        name = name.substring(1);
    }

    // get the Parameter index
    var ipmtr = -1;      // 0-bias
    var jpmtr;           // 1-bias (and temp)
    for (jpmtr = 0; jpmtr < pmtr.length; jpmtr++) {
        if (pmtr[jpmtr].name == name) {
            ipmtr = jpmtr;
            break;
        }
    }
    if (ipmtr < 0) {
        alert(name+" not found");
        return;
    } else {
        jpmtr = ipmtr + 1;
    }

    // get each of the values
    var index  = -1;
    var update = 0;
    for (var irow = 1; irow <= pmtr[ipmtr].nrow; irow++) {
        for (var icol = 1; icol <= pmtr[ipmtr].ncol; icol++) {
            index++;

            // get the new value
            var newValue;
            if (pmtr[ipmtr].nrow == 1 && pmtr[ipmtr].ncol == 1) {
                newValue = prompt("Enter new value for "+name,                       pmtr[ipmtr].value[index]);
            } else {
                newValue = prompt("Enter new value for "+name+"["+irow+","+icol+"]", pmtr[ipmtr].value[index]);
            }

            // make sure a valid number was entered
            if (newValue === null) {
                continue;
            } else if (isNaN(newValue)) {
                alert("Illegal number format, so value not being changed");
                continue;
            } else if (newValue == pmtr[ipmtr].value[index]) {
                continue;
            } else if (pmtr[ipmtr].nrow == 1 && pmtr[ipmtr].ncol == 1) {
                postMessage("Parameter '"+name+"' has been changed to "+newValue+" ====> Re-build is needed <====");
                update++;
            } else {
                postMessage("Parameter '"+name+"["+irow+","+icol+"]' has been changed to "+newValue+" ====> Re-build is needed <====");
                update++;
            }

            // store the value locally
            pmtr[ipmtr].value[index] = Number(newValue);

            // send the new value to the server
            g.socketUt.send("setPmtr;"+jpmtr+";"+irow+";"+icol+";"+newValue+";");

        }
    }

    // update the UI
    if (update > 0) {
        var myElem = document.getElementById(id);
        myElem.className = "fakelinkoff";

        if (pmtr[ipmtr].nrow == 1 && pmtr[ipmtr].ncol == 1) {
            myElem = document.getElementById(id.replace(/col2/, "col3"));
            myElem["innerHTML"] = newValue;
        }

        activateBuildButton();
    }
}


//
// called when "Add Branch at end" is selected in brch1_cmenu (defined in ESP.html)
// called when "Add Branch after"  is selected in brch2_cmenu (defined in ESP.html)
//
function addBrch(e) {
    // alert("in addBrch("+e+")");

    if (g.server != "serveCSM") {
        alert("addBrch is not implemented for "+g.server);
        return;
    }

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    var ibrch = -1;
    var name  = "";

    // get the Tree node
    var inode = Number(id.substring(4,id.length-4));

    // if this was called by pressing "Add Branch at end", new
    //    Branch will be added at end
    if (inode == 2) {
        ibrch = brch.length;
        if (ibrch > 0) {
            name = brch[ibrch-1].name;
        } else {
            name = "at beginning";
        }

    // otherwise, find ibrch
    } else {

        // get the Branch name
        name = myTree.name[inode];
        while (name.charAt(0) == "_") {
            name = name.substring(1);
        }

        // get the Branch index
        for (var jbrch = 0; jbrch < brch.length; jbrch++) {
            if (brch[jbrch].name == name) {
                ibrch = jbrch + 1;
            }
        }
        if (ibrch <= 0) {
            alert(name+" not found");
        }
    }

    // post the dialog (centered on screen)
    var body    = document.getElementById("mainBody");
    var left    = body.clientWidth  / 2 - 150;
    var top     = body.clientHeight / 2 -  40;

    var prevReturnValue = window.returnValue;
    window.returnValue = undefined;

    var outputs = showModalDialog("ESP-addBrch.html", null,
        "dialogWidth:300px; dialogHeight:80px; dialogLeft:"+left+"px; dialogTop:"+top+"px");

    if (outputs === undefined) {
        outputs = window.returnValue;   // needed for Google Chrome bug
    }
    window.returnValue = prevReturnValue;

    // extract the outputs
    if (outputs) {
        var type;
        if (outputs == "") {
            return;
        } else {
            type = outputs;
        }

        var inputs = buildInputsObject(type);

        inputs["brchName"] = "**name_automatically_assigned**";
        inputs["brchType"] = type;
        inputs["activity"] = "active";

        if (inputs["argName1"]) inputs["argValu1"] = "";
        if (inputs["argName2"]) inputs["argValu2"] = "";
        if (inputs["argName3"]) inputs["argValu3"] = "";
        if (inputs["argName4"]) inputs["argValu4"] = "";
        if (inputs["argName5"]) inputs["argValu5"] = "";
        if (inputs["argName6"]) inputs["argValu6"] = "";
        if (inputs["argName7"]) inputs["argValu7"] = "";
        if (inputs["argName8"]) inputs["argValu8"] = "";
        if (inputs["argName9"]) inputs["argValu9"] = "";

        // fill in defaults (see OpenCSM.h for details)
        if        (type == "fillet") {
            inputs["argValu2"] = "$0";
        } else if (type == "chamfer") {
            inputs["argValu2"] = "$0";
        } else if (type == "hollow") {
            inputs["argValu2"] = "0";
            inputs["argValu3"] = "0";
            inputs["argValu4"] = "0";
            inputs["argValu5"] = "0";
            inputs["argValu6"] = "0";
            inputs["argValu7"] = "0";
        } else if (type == "intersect") {
            inputs["argValu1"] = "$none";
            inputs["argValu2"] = "1";
        } else if (type == "subtract") {
            inputs["argValu1"] = "$none";
            inputs["argValu2"] = "1";
        } else if (type == "dump") {
            inputs["argValu2"] = "0";
        }

        // post the dialog (centered on screen)
        body    = document.getElementById("mainBody");
        left    = body.clientWidth  / 2 - 200;
        top     = body.clientHeight / 2 - 200;

        var str  = JSON.stringify(inputs);
        var url  = "ESP-editBrch.html?"+str.substring(1, str.length-1);

        prevReturnValue = window.returnValue;
        window.returnValue = undefined;

        outputs = showModalDialog(url, null,
            "dialogWidth:400px; dialogHeight:400px; dialogLeft:"+left+"px; dialogTop:"+top+"px");

        if (outputs === undefined) {
            outputs = window.returnValue;   // needed for Google Chrome bug
        }
        window.returnValue = prevReturnValue;

        // extract the outputs
        if (outputs) {

            // send the new values to the server
            var mesg = "newBrch;"+ibrch+";"+type+";";

            if (outputs["argValu1"]) mesg = mesg + outputs["argValu1"] + ";";
            if (outputs["argValu2"]) mesg = mesg + outputs["argValu2"] + ";";
            if (outputs["argValu3"]) mesg = mesg + outputs["argValu3"] + ";";
            if (outputs["argValu4"]) mesg = mesg + outputs["argValu4"] + ";";
            if (outputs["argValu5"]) mesg = mesg + outputs["argValu5"] + ";";
            if (outputs["argValu6"]) mesg = mesg + outputs["argValu6"] + ";";
            if (outputs["argValu7"]) mesg = mesg + outputs["argValu7"] + ";";
            if (outputs["argValu8"]) mesg = mesg + outputs["argValu8"] + ";";
            if (outputs["argValu9"]) mesg = mesg + outputs["argValu9"] + ";";

            g.socketUt.send(mesg);

            // get an updated version of the Branches
            g.brchStat = 0;

            // update the UI
            postMessage("Branch (type="+type+") has been added after "+name+"  ====> Re-build is needed <====");
            activateBuildButton();
        }
    }
}


//
// called when "Delete last Branch" is selected in brch1_cmenu (defined in ESP.html)
// called when "Delete this Branch" is selected in brch2_cmenu (defined in ESP.html)
//
function delBrch(e) {
    // alert("in delBrch("+e+")");

    if (g.server != "serveCSM") {
        alert("delBrch is not implemented for "+g.server);
        return;
    }

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    var ibrch = -1;
    var name  = "";

    // get the Tree node
    var inode = Number(id.substring(4,id.length-4));

    // if this was called by pressing "Delete last Branch", adjust inode
    //    to point to last visible node (as if "Delete this Branch"
    //    was pressed on the last Branch)
    if (inode == 2) {
        inode = myTree.child[inode];

        while (myTree.next[inode] >= 0) {
            inode = myTree.next[inode];
        }
    }

    // get the Branch name
    name = myTree.name[inode];
    while (name.charAt(0) == "_") {
        name = name.substring(1);
    }

    // get the Branch index
    for (var jbrch = 0; jbrch < brch.length; jbrch++) {
        if (brch[jbrch].name == name) {
            ibrch = jbrch + 1;
        }
    }
    if (ibrch <= 0) {
        alert(name+" not found");
        return;
    }

    // send the message to the server
    g.socketUt.send("delBrch;"+ibrch+";");

    // hide the Tree node that was just updated
    var element = myTree.document.getElementById("node"+inode);
    element.style.display = "none";

    // get an updated version of the Branches
    g.brchStat = 0;

    // update the UI
    postMessage("Deleting Branch "+name+" ====> Re-build is needed <====");
    activateBuildButton();
}


//
// called when "Edit" is selected in brch2_cemnu (defined in ESP.html)
//
function editBrch(e) {
    // alert("in editBrch("+e+")");

    if (g.server != "serveCSM") {
        alert("editBrch is not implemented for "+g.server);
        return;
    }

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    // get the Tree node
    var inode = Number(id.substring(4,id.length-4));

    // get the Branch name
    var name = myTree.name[inode];
    while (name.charAt(0) == "_") {
        name = name.substring(1);
    }

    // get the Branch index
    var ibrch = -1;           // 0-bias
    var jbrch;                // 1-bias (and temp)
    for (jbrch = 0; jbrch < brch.length; jbrch++) {
        if (brch[jbrch].name == name) {
            ibrch = jbrch;
            break;
        }
    }
    if (ibrch < 0) {
        alert(name+" not found");
        return;
    } else {
        jbrch = ibrch + 1;
    }

    var type = brch[ibrch].type;

    // set up inputs object
    var inputs = buildInputsObject(type);

    inputs["brchName"] = name;
    inputs["brchType"] = type;

    if (brch[ibrch].type != "box"       &&
        brch[ibrch].type != "sphere"    &&
        brch[ibrch].type != "cone"      &&
        brch[ibrch].type != "cylinder"  &&
        brch[ibrch].type != "torusr"    &&
        brch[ibrch].type != "import"    &&
        brch[ibrch].type != "udprim"    &&
        brch[ibrch].type != "extrude"   &&
        brch[ibrch].type != "loft"      &&
        brch[ibrch].type != "revolve"   &&
        brch[ibrch].type != "fillet"    &&
        brch[ibrch].type != "chamfer"   &&
        brch[ibrch].type != "hollow"    &&
        brch[ibrch].type != "translate" &&
        brch[ibrch].type != "rotatex"   &&
        brch[ibrch].type != "rotatey"   &&
        brch[ibrch].type != "rotatez"   &&
        brch[ibrch].type != "scale"       ) {
        inputs["activity"] = "none";
    } else if (brch[ibrch].actv == 301) {
        inputs["activity"] = "suppressed";
    } else {
        inputs["activity"] = "active";
    }

    if (brch[ibrch].args.length > 0) inputs["argValu1"] = brch[ibrch].args[0];
    if (brch[ibrch].args.length > 1) inputs["argValu2"] = brch[ibrch].args[1];
    if (brch[ibrch].args.length > 2) inputs["argValu3"] = brch[ibrch].args[2];
    if (brch[ibrch].args.length > 3) inputs["argValu4"] = brch[ibrch].args[3];
    if (brch[ibrch].args.length > 4) inputs["argValu5"] = brch[ibrch].args[4];
    if (brch[ibrch].args.length > 5) inputs["argValu6"] = brch[ibrch].args[5];
    if (brch[ibrch].args.length > 6) inputs["argValu7"] = brch[ibrch].args[6];
    if (brch[ibrch].args.length > 7) inputs["argValu8"] = brch[ibrch].args[7];
    if (brch[ibrch].args.length > 8) inputs["argValu9"] = brch[ibrch].args[8];

    // post the dialog (centered on screen)
    var body = document.getElementById("mainBody");
    var left = body.clientWidth  / 2 - 200;
    var top  = body.clientHeight / 2 - 200;

    var str  = JSON.stringify(inputs);
    var url  = "ESP-editBrch.html?"+str.substring(1, str.length-1);

    var prevReturnValue = window.returnValue;
    window.returnValue = undefined;

    var outputs = showModalDialog(url, null,
        "dialogWidth:400px; dialogHeight:400px; dialogLeft:"+left+"px; dialogTop:"+top+"px");

    if (outputs === undefined) {
        outputs = window.returnValue;   // needed for Google Chrome bug
    }
    window.returnValue = prevReturnValue;

    // extract the outputs
    var update     = 0;
    var updateMesg = "setBrch;"+jbrch+";";
    if (outputs) {
        if (inputs["brchName"] != outputs["brchName"]) {

            // make sure that name does not start with "Brch_"
            if (outputs["brchName"].substring(0,5) == "Brch_") {
                alert("Changed name '"+outputs["brchName"]+"' cannot begin with 'Brch_'");
                return;
            }

            // make sure that name does not already exist
            for (var kbrch = 0; kbrch < brch.length; kbrch++) {
                if (brch[kbrch].name == outputs["brchName"]) {
                    alert("Name '"+outputs["brchName"]+"' already exists");
                    return;
                }
            }

            // store the value locally
            brch[ibrch].name = outputs["brchName"];
            postMessage("Changing name '"+inputs["brchName"]+"' to '"+outputs["brchName"]+"' ====> Re-build is needed <====");

            // update the name in the UI
            var myElem = document.getElementById(id);
            myElem["innerHTML"] = "__"+brch[ibrch].name;

            update++;
        }
        updateMesg = updateMesg + outputs["brchName"] + ";";

        // update the activity
        if (inputs["activity"] != "none"             &&
            inputs["activity"] != outputs["activity"]   ) {

            // store the value locally
            brch[ibrch].actv = outputs["activity"];
            if (brch[ibrch].actv == "active") {
                postMessage("Activating Branch "+name+" ====> Re-build is needed <====");
            } else {
                postMessage("Suppressing Branch "+name+" ====> Re-build is needed <====");
            }

            // get an updated version of the Branches
            g.brchStat = 0;

            update++;
        }
        updateMesg = updateMesg + outputs["activity"] + ";";

        // update any necessary arguments
        for (var i = 0; i <= 8; i++) {
            var arg = "argValu"+(i+1);
            if (outputs[arg]) {
                if (inputs[arg] != outputs[arg]) {

                    // store the value locally
                    brch[ibrch].args[i] = outputs[arg];
                    postMessage("Changing "+name+":"+inputs["argName"+(i+1)]+" to '"+outputs[arg]+"' ====> Re-build is needed <====");
                    update++;
                }
                updateMesg = updateMesg + outputs[arg] + ";";
            } else {
                updateMesg = updateMesg + ";";
            }
        }

        if (update > 0) {
            // send the new values to the server
            g.socketUt.send(updateMesg);

            // update the UI
            var myElem = document.getElementById(id);
            myElem.className = "fakelinkoff";

            activateBuildButton();
        }
    }
}


//
// called when "Add Attribute" is selected in brch2_cemnu (defined in ESP.html)
//
function addAttr(e) {
    // alert("in addAttr("+e+")");

    if (g.server != "serveCSM") {
        alert("addAttr is not implemented for "+g.server);
        return;
    }

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    // get the Tree node
    var inode = Number(id.substring(4,id.length-4));

    // get the Branch name
    var name = myTree.name[inode];
    while (name.charAt(0) == "_") {
        name = name.substring(1);
    }

    // get the Branch index
    var ibrch = -1;           // 0-bias
    var jbrch;                // 1-bias (and temp)
    for (jbrch = 0; jbrch < brch.length; jbrch++) {
        if (brch[jbrch].name == name) {
            ibrch = jbrch;
            break;
        }
    }
    if (ibrch < 0) {
        alert(name+" not found");
        return;
    } else {
        jbrch = ibrch + 1;
    }

    // get the Attribute name and value
    var aname  = prompt("Enter Attribute name");
    if (aname === null) {
        return;
    } else if (aname.length <= 0) {
        return;
    }
    var avalue = prompt("Enter Attribute value");
    if (avalue === null) {
        return;
    } else if (avalue.length <= 0) {
        return;
    }

    // send the new value to the server
    var mesg = "setAttr;"+jbrch+";"+aname+";"+avalue+";";

    g.socketUt.send(mesg);

    // update the UI
    postMessage("Adding attribute \""+aname+"\" (with value "+avalue+") to "+name+" ====> Re-build is needed <====");
    activateBuildButton();

    // get an updated version of the Branches
    g.brchStat = 0;
}


//
// called when "Edit" is selected in attr2_cmenu  (defined in ESP.html)
//
function editAttr(e) {
    // alert("in editAttr("+e+")");

    if (g.server != "serveCSM") {
        alert("editBrch is not implemented for "+g.server);
        return;
    }

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    // get the Tree node
    var inode = Number(id.substring(4,id.length-4));

    // get the Attribute name and value
    var aname  = myTree.name[inode];
    var avalue = myTree.prop1[inode];
    while (aname.charAt(0) == "_") {
        aname = aname.substring(1);
    }

    // get the Branch name
    inode = myTree.parent[inode];

    var name = myTree.name[inode];
    while (name.charAt(0) == "_") {
        name = name.substring(1);
    }

    // get the Branch index
    var ibrch = -1;           // 0-bias
    var jbrch;                // 1-bias (and temp)
    for (jbrch = 0; jbrch < brch.length; jbrch++) {
        if (brch[jbrch].name == name) {
            ibrch = jbrch;
            break;
        }
    }
    if (ibrch < 0) {
        alert(name+" not found");
        return;
    } else {
        jbrch = ibrch + 1;
    }

    // get the new Attribute value
    var anew = prompt(":Enter new value for \""+aname+"\"", avalue);
    if (anew === null) {
        return;
    } else if (anew.length <= 0) {
        return;
    } else if (anew == avalue) {
        return;
    }

    // send the new value to the server
    var mesg = "setAttr;"+jbrch+";"+aname+";"+anew+";";

    g.socketUt.send(mesg);

    // update the UI
    postMessage("Changing attribute \""+aname+"\" to "+anew+" ====> Re-build is needed <====");
    activateBuildButton();

    // get an updated version of the Branches
    g.brchStat = 0;
}


//
// called when "Build to the Branch" is selected in brch2_cmenu (defined in ESP.html)
//
function buildTo(e) {
    // alert("buildTo("+g.cmenuId+") called");

    if (g.server != "serveCSM") {
        alert("buildTo is not implemented for "+g.server);
        return;
    }

    var id;
    if (e !== undefined) {
        id = e["target"].id;
    } else {
        id        = g.cmenuId;
        g.cmenuId = undefined;
    }

    var ibrch = -1;
    var name  = "";

    // get the Tree node
    var inode = Number(id.substring(4,id.length-4));

    // get the Branch name
    name = myTree.name[inode];
    while (name.charAt(0) == "_") {
        name = name.substring(1);
    }

    // get the Branch index
    for (var jbrch = 0; jbrch < brch.length; jbrch++) {
        if (brch[jbrch].name == name) {
            ibrch = jbrch + 1;
        }
    }
    if (ibrch <= 0) {
        alert(name+" not found");
        return;
    }

    // send the message to the server
    g.socketUt.send("build;"+ibrch+";");

    // update the UI
    postMessage("Re-building only to "+name+"...");

    var button = document.getElementById("buildButton");
    button.onclick = null;
    button["innerHTML"] = "Re-bulding...";
    button.style.backgroundColor = "#FFFF3F";
}


//
// called when "oncontextmenu" event occurs (defined in ESP.html)
//
function rememberCmenuId(e) {
    if (e["originalTarget"].nodeName == "TD") {
        g.cmenuId = e["originalTarget"].id;
    } else {
        g.cmenuId = undefined;
    }
}


//
// callback to get the cursor location
//
function getCursorXY(e)
{
    if (!e) var e = event;

    g.cursorX  =  e.clientX - g.offLeft            - 1;
    g.cursorY  = -e.clientY + g.offTop  + g.height + 1;

                    g.modifier  = 0;
    if (e.shiftKey) g.modifier |= 1;
    if (e.altKey  ) g.modifier |= 2;
    if (e.ctrlKey ) g.modifier |= 4;
}


//
// callback when any mouse is pressed in canvas
//
function getMouseDown(e)
{
    if (!e) var e = event;

    g.startX   =  e.clientX - g.offLeft            - 1;
    g.startY   = -e.clientY + g.offTop  + g.height + 1;

    g.dragging = true;
    g.button   = e.button;

                    g.modifier  = 0;
    if (e.shiftKey) g.modifier |= 1;
    if (e.altKey  ) g.modifier |= 2;
    if (e.ctrlKey ) g.modifier |= 4;
}


//
// callback when the mouse is released
//
function getMouseUp(e)
{
    g.dragging = false;
}


//
// callback when the mouse leaves the canvas
//
function mouseLeftCanvas(e)
{
    if (g.dragging) {
        g.dragging = false;
    }
}


//
// callback when a key is pressed
//
function getKeyPress(e)
{
    if (!e) var e = event;

    g.keyPress = e.charCode;

    // make sure that Firefox does not stop websockets if <esc> is pressed
    if (e.charCode == 0) {
        e.preventDefault();
    }
}


//
// callback to toggle Viz property
//
function toggleViz(e) {
    // alert("in toggleViz("+e+")");

    // get the Tree Node
    var inode = e["target"].id.substring(4);
    inode     = inode.substring(0,inode.length-4);
    inode     = Number(inode);

    // toggle the Viz property
    if        (myTree.valu1[inode] == "off") {
        myTree.prop(inode, 1, "on");
    } else if (myTree.valu1[inode] == "on") {
        myTree.prop(inode, 1, "off");
    } else {
        alert("illegal Viz property:"+myTree.valu1[inode]);
        return;
    }
}


//
// callback to toggle Grd property
//
function toggleGrd(e) {
    // alert("in toggleGrd("+e+")");

    // get the Tree Node
    var inode = e["target"].id.substring(4);
    inode     = inode.substring(0,inode.length-4);
    inode     = Number(inode);

    // toggle the Grd property
    if        (myTree.valu2[inode] == "off") {
        myTree.prop(inode, 2, "on");
    } else if (myTree.valu2[inode] == "on") {
        myTree.prop(inode, 2, "off");
    } else {
        alert("illegal Grd property:"+myTree.valu2[inode]);
        return;
    }
}


//
// callback to toggle Trn property
//
function toggleTrn(e) {
    // alert("in toggleTrn("+e+")");

    // get the Tree Node
    var inode = e["target"].id.substring(4);
    inode     = inode.substring(0,inode.length-4);
    inode     = Number(inode);

    // toggle the Trn property
    if        (myTree.valu3[inode] == "off") {
        myTree.prop(inode, 3, "on");
    } else if (myTree.valu3[inode] == "on") {
        myTree.prop(inode, 3, "off");
    } else {
        alert("illegal Trn property:"+myTree.valu3[inode]);
        return;
    }
}


//
// callback to toggle Ori property
//
function toggleOri(e) {
    // alert("in toggleOri("+e+")");

    // get the Tree Node
    var inode = e["target"].id.substring(4);
    inode     = inode.substring(0,inode.length-4);
    inode     = Number(inode);

    // toggle the Ori property
    if        (myTree.valu3[inode] == "off") {
        myTree.prop(inode, 3, "on");
    } else if (myTree.valu3[inode] == "on") {
        myTree.prop(inode, 3, "off");
    } else {
        alert("illegal Ori property:"+myTree.valu3[inode]);
        return;
    }
}


//
// constructor for a Tree
//
function Tree(doc, treeId) {
    // alert("in Tree("+doc+","+treeId+")");

    // remember the document
    this.document = doc;
    this.treeId   = treeId;

    // arrays to hold the Nodes
    this.name   = new Array();
    this.gprim  = new Array();
    this.click  = new Array();
    this.cmenu  = new Array();
    this.parent = new Array();
    this.child  = new Array();
    this.next   = new Array();
    this.nprop  = new Array();
    this.opened = new Array();

    this.prop1  = new Array();
    this.valu1  = new Array();
    this.cbck1  = new Array();
    this.prop2  = new Array();
    this.valu2  = new Array();
    this.cbck2  = new Array();
    this.prop3  = new Array();
    this.valu3  = new Array();
    this.cbck3  = new Array();

    // initialize Node=0 (the root)
    this.name[  0] = "**root**";
    this.gprim[ 0] = "";
    this.click[ 0] = null;
    this.cmenu[ 0] = "";
    this.parent[0] = -1;
    this.child[ 0] = -1;
    this.next[  0] = -1;
    this.nprop[ 0] =  0;
    this.prop1[ 0] = "";
    this.valu1[ 0] = "";
    this.cbck1[ 0] = null;
    this.prop2[ 0] = "";
    this.valu2[ 0] = "";
    this.cbck2[ 0] = null;
    this.prop3[ 0] = "";
    this.valu3[ 0] = "";
    this.cbck3[ 0] = null;
    this.opened[0] = +1;

    // add methods
    this.addNode  = TreeAddNode;
    this.expand   = TreeExpand;
    this.contract = TreeContract;
    this.prop     = TreeProp;
    this.clear    = TreeClear;
    this.build    = TreeBuild;
    this.update   = TreeUpdate;
}


//
// add a Node to the Tree
//
function TreeAddNode(iparent, name, gprim, click, cmenu, prop1, valu1, cbck1, prop2, valu2, cbck2, prop3, valu3, cbck3) {
    // alert("in TreeAddNode("+iparent+","+name+","+gprim+","+click+","+cmenu+","+prop1+","+valu1+","+cbck1+","+prop2+","+valu2+","+cbck2+","+prop3+","+valu3+","+cbck3+")");

    // validate the input
    if (iparent < 0 || iparent >= this.name.length) {
        alert("iparent="+iparent+" is out of range");
        return;
    }

    // find the next Node index
    var inode = this.name.length;

    // store this Node's values
    this.name[  inode] = name;
    this.gprim[ inode] = gprim;
    this.click[ inode] = click;
    this.cmenu[ inode] = cmenu;
    this.parent[inode] = iparent;
    this.child[ inode] = -1;
    this.next[  inode] = -1;
    this.nprop[ inode] =  0;
    this.opened[inode] =  0;

    // store the properties
    if (prop1 !== undefined) {
        this.nprop[inode] = 1;
        this.prop1[inode] = prop1;
        this.valu1[inode] = valu1;
        this.cbck1[inode] = cbck1;
    }

    if (prop2 !== undefined) {
        this.nprop[inode] = 2;
        this.prop2[inode] = prop2;
        this.valu2[inode] = valu2;
        this.cbck2[inode] = cbck2;
    }

    if (prop3 !== undefined) {
        this.nprop[inode] = 3;
        this.prop3[inode] = prop3;
        this.valu3[inode] = valu3;
        this.cbck3[inode] = cbck3;
    }

    // if the parent does not have a child, link this
    //    new Node to the parent
    if (this.child[iparent] < 0) {
        this.child[iparent] = inode;

    // otherwise link this Node to the last parent's child
    } else {
        var jnode = this.child[iparent];
        while (this.next[jnode] >= 0) {
            jnode = this.next[jnode];
        }

        this.next[jnode] = inode;
    }
}


//
// build the Tree (ie, create the html table from the Nodes)
//
function TreeBuild() {
    // alert("in TreeBuild()");

    var doc = this.document;

    // if the table already exists, delete it and all its children (3 levels)
    var thisTable = doc.getElementById(this.treeId);
    if (thisTable) {
        var child1 = thisTable.lastChild;
        while (child1) {
            var child2 = child1.lastChild;
            while (child2) {
                var child3 = child2.lastChild;
                while (child3) {
                    child2.removeChild(child3);
                    child3 = child2.lastChild;
                }
                child1.removeChild(child2);
                child2 = child1.lastChild;
            }
            thisTable.removeChild(child1);
            child1 = thisTable.lastChild;
        }
        thisTable.parentNode.removeChild(thisTable);
    }

    // build the new table
    var newTable = doc.createElement("table");
    newTable.setAttribute("id", this.treeId);
    doc.getElementById("leftframe").appendChild(newTable);

    // traverse the Nodes using depth-first search
    var inode = 1;
    while (inode > 0) {

        // table row "node"+inode
        var newTR = doc.createElement("TR");
        newTR.setAttribute("id", "node"+inode);
        newTable.appendChild(newTR);

        // table data "node"+inode+"col1"
        var newTDcol1 = doc.createElement("TD");
        newTDcol1.setAttribute("id", "node"+inode+"col1");
        newTDcol1.className = "fakelinkon";
        newTR.appendChild(newTDcol1);

        var newTexta = doc.createTextNode("");
        newTDcol1.appendChild(newTexta);

        // table data "node"+inode+"col2"
        var newTDcol2 = doc.createElement("TD");
        newTDcol2.setAttribute("id", "node"+inode+"col2");
        if (this.cmenu[inode] != "") {
            newTDcol2.className = "fakelinkcmenu";
            newTDcol2.setAttribute("contextmenu", this.cmenu[inode]);
        }
        newTR.appendChild(newTDcol2);

        var newTextb = doc.createTextNode(this.name[inode]);
        newTDcol2.appendChild(newTextb);

        var name = this.name[inode];
        while (name.charAt(0) == "_") {
            name = name.substring(1);
        }

        var ibrch = 0;
        for (var jbrch = 0; jbrch < brch.length; jbrch++) {
            if (brch[jbrch].name == name) {
                if (brch[jbrch].ileft == -2) {
                    newTDcol2.className = "errorTD";
                }
                break;
            }
        }

        // table data "node"+inode+"col3"
        if (this.nprop[inode] > 0) {
            var newTDcol3 = doc.createElement("TD");
            newTDcol3.setAttribute("id", "node"+inode+"col3");
            if (this.cbck1[inode] != "") {
                newTDcol3.className = "fakelinkon";
            }
            newTR.appendChild(newTDcol3);

            if (this.nprop[inode] == 1) {
                newTDcol3.setAttribute("colspan", "3");
            }

            var newTextc = doc.createTextNode(this.prop1[inode]);
            newTDcol3.appendChild(newTextc);
        }

        // table data "node:+inode+"col4"
        if (this.nprop[inode] > 1) {
            var newTDcol4 = doc.createElement("TD");
            newTDcol4.setAttribute("id", "node"+inode+"col4");
            if (this.cbck2[inode] != "") {
                newTDcol4.className = "fakelinkon";
            }
            newTR.appendChild(newTDcol4);

            if (this.nprop[inode] == 2) {
                newTDcol4.setAttribute("colspan", "2");
            }

            var newTextd = doc.createTextNode(this.prop2[inode]);
            newTDcol4.appendChild(newTextd);
        }

        // table data "node:+inode+"col5"
        if (this.nprop[inode] > 2) {
            var newTDcol5 = doc.createElement("TD");
            newTDcol5.setAttribute("id", "node"+inode+"col5");
            if (this.cbck3[inode] != "") {
                newTDcol5.className = "fakelinkon";
            }
            newTR.appendChild(newTDcol5);

            var newTextd = doc.createTextNode(this.prop3[inode]);
            newTDcol5.appendChild(newTextd);
        }

        // go to next row
        if        (this.child[inode] >= 0) {
            inode = this.child[inode];
        } else if (this.next[inode] >= 0) {
            inode = this.next[inode];
        } else {
            while (inode > 0) {
                inode = this.parent[inode];
                if (this.parent[inode] == 0) {
                    newTR = doc.createElement("TR");
                    newTR.setAttribute("height", "10px");
                    newTable.appendChild(newTR);
                }
                if (this.next[inode] >= 0) {
                    inode = this.next[inode];
                    break;
                }
            }
        }
    }

    this.update();
}


//
// clear the Tree
//
function TreeClear() {
    // alert("in TreeClear()");

    // remove all but the first Node
    this.name.splice(  1);
    this.gprim.splice( 1);
    this.click.splice( 1);
    this.cmenu.splice( 1);
    this.parent.splice(1);
    this.child.splice( 1);
    this.next.splice(  1);
    this.nprop.splice( 1);
    this.opened.splice(1);

    this.prop1.splice(1);
    this.valu1.splice(1);
    this.cbck1.splice(1);
    this.prop2.splice(1);
    this.valu2.splice(1);
    this.cbck2.splice(1);
    this.prop3.splice(1);
    this.valu3.splice(1);
    this.cbck3.splice(1);

    // reset the root Node
    this.parent[0] = -1;
    this.child[ 0] = -1;
    this.next[  0] = -1;
}


//
// expand a Node in the Tree
//
function TreeContract(inode) {
    // alert("in TreeContract("+inode+")");

    // validate inputs
    if (inode < 0 || inode >= this.opened.length) {
        alert("inode="+inode+" is out of range");
        return;
    }

    // contract inode
    this.opened[inode] = 0;

    // contract all descendents of inode
    for (var jnode = 1; jnode < this.parent.length; jnode++) {
        var iparent = this.parent[jnode];
        while (iparent > 0) {
            if (iparent == inode) {
                this.opened[jnode] = 0;
                break;
            }

            iparent = this.parent[iparent];
        }
    }

    // update the display
    this.update();
}


//
// expand a Node in the Tree
//
function TreeExpand(inode) {
    // alert("in TreeExpand("+inode+")");

    // validate inputs
    if (inode < 0 || inode >= this.opened.length) {
        alert("inode="+inode+" is out of range");
        return;
    }

    // expand inode
    this.opened[inode] = 1;

    // update the display
    this.update();
}


//
// change a property of a Node
//
function TreeProp(inode, iprop, onoff) {
    // alert("in TreeProp("+inode+","+iprop+","+onoff+")");

    // validate inputs
    if (inode < 0 || inode >= this.opened.length) {
        alert("inode="+inode+" is out of range");
        return;
    } else if (onoff != "on" && onoff != "off") {
        alert("onoff="+onoff+" is not 'on' or 'off'");
        return;
    }

    // set the property for inode
    if (iprop == 1) {
        this.valu1[inode] = onoff;
    } else if (iprop == 2) {
        this.valu2[inode] = onoff;
    } else if (iprop == 3) {
        this.valu3[inode] = onoff;
    } else {
        alert("iprop="+iprop+" is not 1, 2, or 3");
        return;
    }

    // set property of all descendents of inode
    for (var jnode = 1; jnode < this.parent.length; jnode++) {
        var iparent = this.parent[jnode];
        while (iparent > 0) {
            if (iparent == inode) {
                if        (iprop == 1) {
                    this.valu1[jnode] = onoff;
                } else if (iprop == 2) {
                    this.valu2[jnode] = onoff;
                } else if (iprop == 3) {
                    this.valu3[jnode] = onoff;
                }
                break;
            }

            iparent = this.parent[iparent];
        }
    }

    this.update();
}


//
// update the Tree (after build/expension/contraction/property-set)
//
function TreeUpdate() {
    // alert("in TreeUpdate()");

    var doc = this.document;

    // traverse the Nodes using depth-first search
    for (var inode = 1; inode < this.opened.length; inode++) {
        var element = doc.getElementById("node"+inode);

        // unhide the row
        element.style.display = null;

        // hide the row if one of its parents has .opened=0
        var jnode = this.parent[inode];
        while (jnode != 0) {
            if (this.opened[jnode] == 0) {
                element.style.display = "none";
                break;
            }

            jnode = this.parent[jnode];
        }

        // if the current Node has children, set up appropriate event handler to expand/collapse
        if (this.child[inode] > 0) {
            if (this.opened[inode] == 0) {
                var myElem = doc.getElementById("node"+inode+"col1");
                var This   = this;

                myElem.firstChild.nodeValue = "+";
                myElem.onclick = function () {
                    var thisNode = this.id.substring(4);
                    thisNode     = thisNode.substring(0,thisNode.length-4);
                    This.expand(thisNode);
                };

            } else {
                var myElem = doc.getElementById("node"+inode+"col1");
                var This   = this;

                myElem.firstChild.nodeValue = "-";
                myElem.onclick = function () {
                    var thisNode = this.id.substring(4);
                    thisNode     = thisNode.substring(0,thisNode.length-4);
                    This.contract(thisNode);
                };
            }
        }

        if (this.click[inode] !== null) {
            var myElem = doc.getElementById("node"+inode+"col2");
            myElem.onclick = this.click[inode];
        }

        // set the class of the properties
        if (this.nprop[inode] >= 1) {
            var myElem = doc.getElementById("node"+inode+"col3");
            myElem.onclick = this.cbck1[inode];

            if (this.prop1[inode] == "Viz") {
                if (this.valu1[inode] == "off") {
                    myElem.setAttribute("class", "fakelinkoff");
                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs &= ~g.plotAttrs.ON;
                        g.sceneUpd = 1;
                    }
                } else {
                    myElem.setAttribute("class", "fakelinkon");
                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs |=  g.plotAttrs.ON;
                        g.sceneUpd = 1;
                    }
                }
            }
        }

        if (this.nprop[inode] >= 2) {
            var myElem = doc.getElementById("node"+inode+"col4");
            myElem.onclick = this.cbck2[inode];

            if (this.prop2[inode] == "Grd") {
                if (this.valu2[ inode] == "off") {
                    myElem.setAttribute("class", "fakelinkoff");

                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs &= ~g.plotAttrs.LINES;
                        g.sceneGraph[this.gprim[inode]].attrs &= ~g.plotAttrs.POINTS;
                        g.sceneUpd = 1;
                    }
                } else {
                    myElem.setAttribute("class", "fakelinkon");

                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs |=  g.plotAttrs.LINES;
                        g.sceneGraph[this.gprim[inode]].attrs |=  g.plotAttrs.POINTS;
                        g.sceneUpd = 1;
                    }
                }
            }
        }

        if (this.nprop[inode] >= 3) {
            var myElem = doc.getElementById("node"+inode+"col5");
            myElem.onclick = this.cbck3[inode];

            if (this.prop3[inode] == "Trn") {
                if (this.valu3[ inode] == "off") {
                    myElem.setAttribute("class", "fakelinkoff");

                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs &= ~g.plotAttrs.TRANSPARENT;
                        g.sceneUpd = 1;
                    }
                } else {
                    myElem.setAttribute("class", "fakelinkon");

                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs |=  g.plotAttrs.TRANSPARENT;
                        g.sceneUpd = 1;
                    }
                }
            } else if (this.prop3[inode] == "Ori") {
                if (this.valu3[ inode] == "off") {
                    myElem.setAttribute("class", "fakelinkoff");

                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs &= ~g.plotAttrs.ORIENTATION;
                        g.sceneUpd = 1;
                    }
                } else {
                    myElem.setAttribute("class", "fakelinkon");

                    if (this.gprim[inode] != "") {
                        g.sceneGraph[this.gprim[inode]].attrs |=  g.plotAttrs.ORIENTATION;
                        g.sceneUpd = 1;
                    }
                }
            }
        }
    }
}


//
// post a message into the botmframe
//
function postMessage(mesg) {
    // alert("in postMessage("+mesg+")");

    var botm = document.getElementById("botmframe");

    var text = document.createTextNode(mesg);
    botm.insertBefore(text, botm.lastChild);

    var br = document.createElement("br");
    botm.insertBefore(br, botm.lastChild);

    br.scrollIntoView();
}


//
// called when "onresize" event occurs (defined in ESP.html)
// resize the frames (with special handling to width of leftFrame and height of botmframe
//
function resizeFrames() {
    // alert("resizeFrames");

    var scrollSize;
    if (BrowserDetect.browser == "Chrome") {
        scrollSize = 24;
    } else {
        scrollSize = 20;
    }

    // get the size of the client (minus amount to account for scrollbars)
    var body = document.getElementById("mainBody");
    var bodyWidth  = body.clientWidth  - scrollSize;
    var bodyHeight = body.clientHeight - scrollSize;

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
    var canvWidth = riteWidth - scrollSize;

    left.style.width = leftWidth+"px";
    rite.style.width = riteWidth+"px";
    botm.style.width = bodyWidth+"px";
    canv.style.width = canvWidth+"px";
    canv.width       = canvWidth;

    // compute and set the heights of the frames
    //    (do not make botm frame larger than 200px)
    var botmHeight = Math.round(0.20 * bodyHeight);
    if (botmHeight > 200)   botmHeight = 200;
    var  topHeight = bodyHeight - botmHeight;
    var canvHeight =  topHeight - scrollSize - 5;

    left.style.height =  topHeight+"px";
    rite.style.height =  topHeight+"px";
    botm.style.height = botmHeight+"px";
    canv.style.height = canvHeight+"px";
    canv.height       = canvHeight;
}


//
// called when the canvas size changes or relocates (called by wv-draw.js)
//
function reshape(gl)
{
    // alert("reshape("+gl+")");

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


//
// build the inputs object given the Branch type
//
function buildInputsObject(type)
{
    // alert("buildInputsArray("+type+")");

    var inputs = {};

    if        (type == "set") {
        inputs["argName1"] = "$pmtrName";
        inputs["argName2"] = "$exprs";
    } else if (type == "box") {
        inputs["argName1"] = "xmin";
        inputs["argName2"] = "ymin";
        inputs["argName3"] = "zmin";
        inputs["argName4"] = "xsize";
        inputs["argName5"] = "ysize";
        inputs["argName6"] = "zsize";
    } else if (type == "sphere") {
        inputs["argName1"] = "xcent";
        inputs["argName2"] = "ycent";
        inputs["argName3"] = "zcent";
        inputs["argName4"] = "radius";
    } else if (type == "cone") {
        inputs["argName1"] = "xvrtx";
        inputs["argName2"] = "yvrtx";
        inputs["argName3"] = "zvrtz";
        inputs["argName4"] = "xbase";
        inputs["argName5"] = "ybase";
        inputs["argName6"] = "zbase";
        inputs["argName7"] = "radius";
    } else if (type == "cylinder") {
        inputs["argName1"] = "xbeg";
        inputs["argName2"] = "ybeg";
        inputs["argName3"] = "zbeg";
        inputs["argName4"] = "xend";
        inputs["argName5"] = "yend";
        inputs["argName6"] = "zend";
        inputs["argName7"] = "radius";
    } else if (type == "torus") {
        inputs["argName1"] = "xcent";
        inputs["argName2"] = "ycent";
        inputs["argName3"] = "zcent";
        inputs["argName4"] = "dxaxis";
        inputs["argName5"] = "dyaxis";
        inputs["argName6"] = "dzaxis";
        inputs["argName7"] = "majorRad";
        inputs["argName8"] = "minorRad";
    } else if (type == "import") {
        inputs["argName1"] = "$filename";
    } else if (type == "udprim") {
        inputs["argName1"] = "$primtype";
        inputs["argName2"] = "$argName1";
        inputs["argName3"] = "$argType1";
        inputs["argName4"] = "$argName2";
        inputs["argName5"] = "$argType2";
        inputs["argName6"] = "$argName3";
        inputs["argName7"] = "$argType3";
        inputs["argName8"] = "$argName4";
        inputs["argName9"] = "$argType4";
    } else if (type == "extrude") {
        inputs["argName1"] = "dx";
        inputs["argName2"] = "dy";
        inputs["argName3"] = "dz";
    } else if (type == "loft") {
        inputs["argName1"] = "smooth";
    } else if (type == "revolve") {
        inputs["argName1"] = "xorig";
        inputs["argName2"] = "yorig";
        inputs["argName3"] = "zorig";
        inputs["argName4"] = "dxaxis";
        inputs["argName5"] = "dyaxis";
        inputs["argName6"] = "dzaxis";
        inputs["argName7"] = "angDeg";
    } else if (type == "fillet") {
        inputs["argName1"] = "radius";
        inputs["argName2"] = "$edgeList";
    } else if (type == "chamfer") {
        inputs["argName1"] = "radius";
        inputs["argName2"] = "$edgeList";
    } else if (type == "hollow") {
        inputs["argName1"] = "thick";
        inputs["argName2"] = "iface1";
        inputs["argName3"] = "iface2";
        inputs["argName4"] = "iface3";
        inputs["argName5"] = "iface4";
        inputs["argName6"] = "iface5";
        inputs["argName7"] = "iface6";
    } else if (type == "intersect") {
        inputs["argName1"] = "$order";
        inputs["argName2"] = "index";
    } else if (type == "subtract") {
        inputs["argName1"] = "$order";
        inputs["argName2"] = "index";
    } else if (type == "union") {
    } else if (type == "translate") {
        inputs["argName1"] = "dx";
        inputs["argName2"] = "dy";
        inputs["argName3"] = "dz";
    } else if (type == "rotatex") {
        inputs["argName1"] = "angDeg";
        inputs["argName2"] = "yaxis";
        inputs["argName3"] = "zaxis";
    } else if (type == "rotatey") {
        inputs["argName1"] = "angDeg";
        inputs["argName2"] = "zaxis";
        inputs["argName3"] = "xaxis";
    } else if (type == "rotatez") {
        inputs["argName1"] = "angDeg";
        inputs["argName2"] = "xaxis";
        inputs["argName3"] = "yaxis";
    } else if (type == "scale") {
        inputs["argName1"] = "fact";
    } else if (type == "skbeg") {
        inputs["argName1"] = "x";
        inputs["argName2"] = "y";
        inputs["argName3"] = "z";
    } else if (type == "linseg") {
        inputs["argName1"] = "x";
        inputs["argName2"] = "y";
        inputs["argName3"] = "z";
    } else if (type == "cirarc") {
        inputs["argName1"] = "xon";
        inputs["argName2"] = "yon";
        inputs["argName3"] = "zon";
        inputs["argName4"] = "xend";
        inputs["argName5"] = "yend";
        inputs["argName6"] = "zend";
    } else if (type == "spline") {
        inputs["argName1"] = "x";
        inputs["argName2"] = "y";
        inputs["argName3"] = "z";
    } else if (type == "skend") {
    } else if (type == "solbeg") {
        inputs["argName1"] = "$varlist";
    } else if (type == "solcon") {
        inputs["argName1"] = "$expr";
    } else if (type == "solend") {
    } else if (type == "macbeg") {
        inputs["argName1"] = "imacro";
    } else if (type == "macend") {
    } else if (type == "recall") {
        inputs["argName1"] = "imacro";
    } else if (type == "patbeg") {
        inputs["argName1"] = "$pmtrName";
        inputs["argName2"] = "ncopy";
    } else if (type == "patend") {
    } else if (type == "mark") {
    } else if (type == "dump") {
        inputs["argName1"] = "$filename";
        inputs["argName2"] = "remove";
    } else {
        alert("'"+type+"' is an unknown type");
        return {};
    }

    return inputs;
}
