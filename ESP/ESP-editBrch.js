// ESP-editBrch.js implements functions for the editBrch dialog
// written by John Dannenhoffer

"use strict";

var numBrchArgs = 0;

//
// Initialize by setting form elements from passed data
//
function initEditBrch()
{
    var inputs = null;
    if (location.search) {
        var json = "{"+location.search.substring(1)+"}";
        json     = json.replace(/%22/g, "\"");
        inputs   = JSON.parse(json);
    } else if (window.dialogArguments) {
        inputs = window.dialogArguments;
    }

    // update the info in the dialog
    if (inputs) {
        document.getElementById("brchType").firstChild["data"]           = inputs["brchType"];
        document.editBrchForm.brchName.value                             = inputs["brchName"];

        if (inputs["argName1"]) {
            numBrchArgs = 1;
            document.getElementById("argName1").firstChild["data"]       = inputs["argName1"];
            if (inputs["argName1"].charAt(0) == "$") {
                document.editBrchForm.argValu1.value                     = inputs["argValu1"].substring(1);
            } else {
                document.editBrchForm.argValu1.value                     = inputs["argValu1"];
            }
        } else {
            document.getElementById("argName1").parentNode.style.display = "none";
        }

        if (inputs["argName2"]) {
            numBrchArgs = 2;
            document.getElementById("argName2").firstChild["data"]       = inputs["argName2"];
            if (inputs["argName2"].charAt(0) == "$") {
                document.editBrchForm.argValu2.value                     = inputs["argValu2"].substring(1);
            } else {
                document.editBrchForm.argValu2.value                     = inputs["argValu2"];
            }
        } else {
            document.getElementById("argName2").parentNode.style.display = "none";
        }

        if (inputs["argName3"]) {
            numBrchArgs = 3;
            document.getElementById("argName3").firstChild["data"]       = inputs["argName3"];
            if (inputs["argName3"].charAt(0) == "$") {
                document.editBrchForm.argValu3.value                     = inputs["argValu3"].substring(1);
            } else {
                document.editBrchForm.argValu3.value                     = inputs["argValu3"];
            }
        } else {
            document.getElementById("argName3").parentNode.style.display = "none";
        }

        if (inputs["argName4"]) {
            numBrchArgs = 4;
            document.getElementById("argName4").firstChild["data"]       = inputs["argName4"];
            if (inputs["argName4"].charAt(0) == "$") {
                document.editBrchForm.argValu4.value                     = inputs["argValu4"].substring(1);
            } else {
                document.editBrchForm.argValu4.value                     = inputs["argValu4"];
            }
        } else {
            document.getElementById("argName4").parentNode.style.display = "none";
        }

        if (inputs["argName5"]) {
            numBrchArgs = 5;
            document.getElementById("argName5").firstChild["data"]       = inputs["argName5"];
            if (inputs["argName5"].charAt(0) == "$") {
                document.editBrchForm.argValu5.value                     = inputs["argValu5"].substring(1);
            } else {
                document.editBrchForm.argValu5.value                     = inputs["argValu5"];
            }
        } else {
            document.getElementById("argName5").parentNode.style.display = "none";
        }

        if (inputs["argName6"]) {
            numBrchArgs = 6;
            document.getElementById("argName6").firstChild["data"]       = inputs["argName6"];
            if (inputs["argName6"].charAt(0) == "$") {
                document.editBrchForm.argValu6.value                     = inputs["argValu6"].substring(1);
            } else {
                document.editBrchForm.argValu6.value                     = inputs["argValu6"];
            }
        } else {
            document.getElementById("argName6").parentNode.style.display = "none";
        }

        if (inputs["argName7"]) {
            numBrchArgs = 7;
            document.getElementById("argName7").firstChild["data"]       = inputs["argName7"];
            if (inputs["argName7"].charAt(0) == "$") {
                document.editBrchForm.argValu7.value                     = inputs["argValu7"].substring(1);
            } else {
                document.editBrchForm.argValu7.value                     = inputs["argValu7"];
            }
        } else {
            document.getElementById("argName7").parentNode.style.display = "none";
        }

        if (inputs["argName8"]) {
            numBrchArgs = 8;
            document.getElementById("argName8").firstChild["data"]       = inputs["argName8"];
            if (inputs["argName8"].charAt(0) == "$") {
                document.editBrchForm.argValu8.value                     = inputs["argValu8"].substring(1);
            } else {
                document.editBrchForm.argValu8.value                     = inputs["argValu8"];
            }
        } else {
            document.getElementById("argName8").parentNode.style.display = "none";
        }

        if (inputs["argName9"]) {
            numBrchArgs = 9;
            document.getElementById("argName9").firstChild["data"]       = inputs["argName9"];
            if (inputs["argName9"].charAt(0) == "$") {
                document.editBrchForm.argValu9.value                     = inputs["argValu9"].substring(1);
            } else {
                document.editBrchForm.argValu9.value                     = inputs["argValu9"];
            }
        } else {
            document.getElementById("argName9").parentNode.style.display = "none";
        }

        if (inputs["activity"]) {
            var element = document.getElementById("activity");
            if (inputs["activity"] != "none") {
                element.style.display = null;
                document.editBrchForm.activity.value = inputs["activity"];
            } else {
                element.style.display = "none";
            }
        }
    }
}

//
// Handle click of Cancel button
//
function handleCancel()
{
    // for browswers that handle .returnValue properly
    window.returnValue = "";

    // if not, attach returnValue to the opener
    if (window.opener) {
        window.opener.returnValue = "";
    }

    window.close();
}

//
// Handle click of OK button
//
function handleOK()
{
    var outputs = {};

    outputs["brchName"] = document.editBrchForm.brchName.value.replace(/\s/g, "");
    if (outputs["brchName"].length <= 0) {
        alert("Name cannot be blank");
        return;
    }

    if (numBrchArgs >= 1) {
        var name1 = document.getElementById("argName1").firstChild["data"];
        var valu1 = document.editBrchForm.argValu1.value.replace(/\s/g, "");

        if (valu1.length <= 0) {
            alert(name1+" cannot be blank");
            return;
        } else if (name1.charAt(0) != "$") {
            outputs["argValu1"] =       valu1;
        } else {
            outputs["argValu1"] = "$" + valu1;
        }
    }

    if (numBrchArgs >= 2) {
        var name2 = document.getElementById("argName2").firstChild["data"];
        var valu2 = document.editBrchForm.argValu2.value.replace(/\s/g, "");

        if (valu2.length <= 0) {
            alert(name2+" cannot be blank");
            return;
        } else if (name2.charAt(0) != "$") {
            outputs["argValu2"] =       valu2;
        } else {
            outputs["argValu2"] = "$" + valu2;
        }
    }

    if (numBrchArgs >= 3) {
        var name3 = document.getElementById("argName3").firstChild["data"];
        var valu3 = document.editBrchForm.argValu3.value.replace(/\s/g, "");

        if (valu3.length <= 0) {
            alert(name3+" cannot be blank");
            return;
        } else if (name3.charAt(0) != "$") {
            outputs["argValu3"] =       valu3;
        } else {
            outputs["argValu3"] = "$" + valu3;
        }
    }

    if (numBrchArgs >= 4) {
        var name4 = document.getElementById("argName4").firstChild["data"];
        var valu4 = document.editBrchForm.argValu4.value.replace(/\s/g, "");

        if (valu4.length <= 0) {
            alert(name4+" cannot be blank");
            return;
        } else if (name4.charAt(0) != "$") {
            outputs["argValu4"] =       valu4;
        } else {
            outputs["argValu4"] = "$" + valu4;
        }
    }

    if (numBrchArgs >= 5) {
        var name5 = document.getElementById("argName5").firstChild["data"];
        var valu5 = document.editBrchForm.argValu5.value.replace(/\s/g, "");

        if (valu5.length <= 0) {
            alert(name5+" cannot be blank");
            return;
        } else if (name5.charAt(0) != "$") {
            outputs["argValu5"] =       valu5;
        } else {
            outputs["argValu5"] = "$" + valu5;
        }
    }

    if (numBrchArgs >= 6) {
        var name6 = document.getElementById("argName6").firstChild["data"];
        var valu6 = document.editBrchForm.argValu6.value.replace(/\s/g, "");

        if (valu6.length <= 0) {
            alert(name6+" cannot be blank");
            return;
        } else if (name6.charAt(0) != "$") {
            outputs["argValu6"] =       valu6;
        } else {
            outputs["argValu6"] = "$" + valu6;
        }
    }

    if (numBrchArgs >= 7) {
        var name7 = document.getElementById("argName7").firstChild["data"];
        var valu7 = document.editBrchForm.argValu7.value.replace(/\s/g, "");

        if (valu7.length <= 0) {
            alert(name7+" cannot be blank");
            return;
        } else if (name7.charAt(0) != "$") {
            outputs["argValu7"] =       valu7;
        } else {
            outputs["argValu7"] = "$" + valu7;
        }
    }

    if (numBrchArgs >= 8) {
        var name8 = document.getElementById("argName8").firstChild["data"];
        var valu8 = document.editBrchForm.argValu8.value.replace(/\s/g, "");

        if (valu8.length <= 0) {
            alert(name8+" cannot be blank");
            return;
        } else if (name8.charAt(0) != "$") {
            outputs["argValu8"] =       valu8;
        } else {
            outputs["argValu8"] = "$" + valu8;
        }
    }

    if (numBrchArgs >= 9) {
        var name9 = document.getElementById("argName9").firstChild["data"];
        var valu9 = document.editBrchForm.argValu9.value.replace(/\s/g, "");

        if (valu9.length <= 0) {
            alert(name9+" cannot be blank");
            return;
        } else if (name9.charAt(0) != "$") {
            outputs["argValu9"] =       valu9;
        } else {
            outputs["argValu9"] = "$" + valu9;
        }
    }

    outputs["activity"] = document.editBrchForm.activity.value.replace(/\s/g, "");

    // for browswers that handle .returnValue properly
    window.returnValue = outputs;

    // if not, attach returnValue to the opener
    if (window.opener) {
        window.opener.returnValue = outputs;
    }

    window.close();
}
