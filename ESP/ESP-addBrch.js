// ESP-addBrch.js implements functions for the addBrch dialog
// written by John Dannenhoffer

"use strict";

//
// Initialize by setting form elements from passed data
//
function initAddBrch()
{
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
    // for browswers that handle .returnValue properly
    window.returnValue = document.addBrchForm.brchType.value;

    // if not, attach returnValue to the opener
    if (window.opener) {
        window.opener.returnValue = document.addBrchForm.brchType.value;
    }

    window.close();
}
