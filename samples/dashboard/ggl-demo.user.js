// ==UserScript==
// @name         GGL Edge AI Dashboard
// @namespace    ggl-demo
// @version      1.1
// @description  Adds Edge AI Dashboard button to AWS console
// @match        https://*.console.aws.amazon.com/*
// @grant        none
// ==/UserScript==

(function() {
    'use strict';

    const DASHBOARD_URL = 'https://d3siolwrdh12qz.cloudfront.net';

    const btn = document.createElement('div');
    btn.innerHTML = '🤖 Edge AI';
    btn.style.cssText = 'position:fixed;bottom:20px;right:20px;z-index:99999;background:#ff9900;color:#000;padding:12px 20px;border-radius:8px;cursor:pointer;font-weight:700;font-size:14px;box-shadow:0 4px 12px rgba(0,0,0,0.3);';
    btn.onclick = () => {
        let frame = document.getElementById('ggl-demo-frame');
        if (frame) { frame.remove(); document.getElementById('ggl-close')?.remove(); return; }
        frame = document.createElement('iframe');
        frame.id = 'ggl-demo-frame';
        frame.src = DASHBOARD_URL;
        frame.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;z-index:99998;border:none;';
        const close = document.createElement('div');
        close.id = 'ggl-close';
        close.innerHTML = '✕ Close';
        close.style.cssText = 'position:fixed;top:12px;right:20px;z-index:99999;background:#ff9900;color:#000;padding:8px 16px;border-radius:6px;cursor:pointer;font-weight:600;';
        close.onclick = () => { frame.remove(); close.remove(); };
        document.body.appendChild(frame);
        document.body.appendChild(close);
    };
    document.body.appendChild(btn);
})();
