// Vessel Command Dashboard
// Served at http://192.168.15.1/ (port 80).
// Polls /data every second for live values from BoatData + LocalSensors.
// To add a new sensor: add its JSON key to handle_data_json() in the .ino,
// then add a DOM element and an update line in the update() function below.

const char indexHTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Vessel Command</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#111827;color:#e1e1e1;height:100vh;display:flex;flex-direction:column;overflow:hidden}

/* ── Header ─────────────────────────────────────── */
.hdr{background:#1c1c1c;padding:10px 16px;display:flex;align-items:center;gap:10px;border-bottom:1px solid #2d2d2d;flex-shrink:0}
.hdr-title{font-size:15px;font-weight:600;color:#fff;flex:1}
.hdr-right{display:flex;align-items:center;gap:6px;font-size:11px;color:#9e9e9e}
#conn-dot{width:9px;height:9px;border-radius:50%;background:#CC0000;transition:background .3s}
#conn-dot.ok{background:#00AA00}
#mqtt-dot{width:9px;height:9px;border-radius:50%;background:#555;transition:background .3s}
#mqtt-dot.ok{background:#00AA00}
#mqtt-dot.warn{background:#FF8800}

/* ── Tab bar ─────────────────────────────────────── */
.tabs{background:#1c1c1c;display:flex;border-bottom:1px solid #2d2d2d;flex-shrink:0;overflow-x:auto}
.tab{flex:1;min-width:72px;padding:10px 4px;text-align:center;cursor:pointer;font-size:11px;font-weight:600;text-transform:uppercase;letter-spacing:.5px;color:#9e9e9e;border-bottom:3px solid transparent;white-space:nowrap;user-select:none}
.tab.active{color:#03a9f4;border-bottom:3px solid #03a9f4}

/* ── Content area ────────────────────────────────── */
.content{flex:1;overflow-y:auto;padding:10px;background:#111827}
.panel{display:none}.panel.active{display:block}

/* ── Grids ───────────────────────────────────────── */
.g2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px}
@media(max-width:540px){
  .g2{grid-template-columns:1fr}
  .g3{grid-template-columns:1fr 1fr}
}
.gap{margin-bottom:10px}

/* ── Instrument card (high-contrast white panel) ─── */
.inst{background:#fff;color:#000;border:4px solid #000;padding:14px}
.lbl{font-size:10px;font-weight:800;text-transform:uppercase;color:#444;letter-spacing:.5px;display:block;margin-bottom:2px}
.val{font-family:'Courier New',monospace;font-weight:900;display:block;line-height:1.05}
.unit{font-size:12px;font-weight:700;display:block;margin-top:2px}

/* ── Dark card ───────────────────────────────────── */
.card{background:#1e2633;border-radius:10px;overflow:hidden;box-shadow:0 2px 6px rgba(0,0,0,.3)}
.card-hdr{padding:9px 12px 3px;font-size:10px;font-weight:600;color:#9e9e9e;text-transform:uppercase;letter-spacing:.5px}
.card-body{padding:10px 12px}

/* ── Bar indicator ───────────────────────────────── */
.bc{background:#1a1a1a;border-radius:7px;padding:9px 12px;margin-bottom:8px}
.bc:last-child{margin-bottom:0}
.bc-name{font-size:10px;color:#9e9e9e;font-weight:600;text-transform:uppercase;letter-spacing:.3px;margin-bottom:5px}
.bc-row{display:flex;align-items:center;gap:8px}
.bc-track{flex:1;height:18px;background:#2a2a2a;border-radius:3px;overflow:hidden}
.bc-fill{height:100%;border-radius:3px;transition:width .5s,background .5s}
.bc-val{font-size:12px;font-weight:700;font-family:monospace;color:#e0e0e0;min-width:68px;text-align:right}
.bc-scale{display:flex;justify-content:space-between;font-size:9px;color:#444;margin-top:2px;font-family:monospace;padding:0 1px}

/* ── Tile ────────────────────────────────────────── */
.tile{border-radius:8px;padding:12px}
.tile-name{font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:.4px;margin-bottom:6px}
.tile-val{font-size:22px;font-weight:900;font-family:monospace}
.tile-sub{font-size:9px;font-weight:700;text-transform:uppercase;margin-top:4px}

/* ── Alarm cards ─────────────────────────────────── */
.alm-act{background:#CC0000;color:#fff;border:4px solid #000;padding:16px 10px;text-align:center;font-weight:900}
.alm-ok{background:#E6FFE6;color:#006600;border:4px solid #006600;padding:16px 10px;text-align:center;font-weight:800;opacity:.8}
.alm-icon{font-size:22px;margin-bottom:4px}
.alm-name{font-size:12px;letter-spacing:.3px}
.alm-detail{font-size:10px;margin-top:5px;opacity:.9}

/* ── Position display ────────────────────────────── */
.pos{font-family:'Courier New',monospace;font-size:13px;font-weight:700;line-height:2;letter-spacing:.3px}

/* ── Misc ────────────────────────────────────────── */
.hint{padding:10px 14px;background:#1e2633;border-radius:8px;font-size:11px;color:#9e9e9e;text-align:center;margin-top:10px}
.content::-webkit-scrollbar{width:4px}
.content::-webkit-scrollbar-track{background:#111}
.content::-webkit-scrollbar-thumb{background:#333;border-radius:2px}
/* ── Mock / no-data indicators ───────────────────── */
#mock-strip{display:none;padding:5px 14px;background:#1c1c1c;font-size:10px;color:#555;text-align:center;text-transform:uppercase;letter-spacing:.7px;border-bottom:1px solid #252525;flex-shrink:0}
body.mock-mode #mock-strip{display:block}
body.mock-mode .val{opacity:.58}
body.mock-mode .bc-fill{opacity:.45}
body.mock-mode .tile-val{opacity:.58}
body.mock-mode .inst{border-color:#777!important;background:#f8f8f8!important}
.nd{display:inline-block;background:#3a0000;color:#FF4444;font-size:11px!important;font-weight:900!important;letter-spacing:.4px;padding:2px 8px;border:1px solid #880000;line-height:1.4;font-family:monospace}

/* ── Chart ───────────────────────────────────────── */
#chart{height:300px;background:#c8d8e4;position:relative}
#chart-msg{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);color:#555;font-size:11px;text-align:center;pointer-events:none;z-index:500;line-height:1.6}
.leaflet-control-attribution{font-size:8px!important}
</style>
<style>
/* required styles */

.leaflet-pane,
.leaflet-tile,
.leaflet-marker-icon,
.leaflet-marker-shadow,
.leaflet-tile-container,
.leaflet-pane > svg,
.leaflet-pane > canvas,
.leaflet-zoom-box,
.leaflet-image-layer,
.leaflet-layer {
	position: absolute;
	left: 0;
	top: 0;
	}
.leaflet-container {
	overflow: hidden;
	}
.leaflet-tile,
.leaflet-marker-icon,
.leaflet-marker-shadow {
	-webkit-user-select: none;
	   -moz-user-select: none;
	        user-select: none;
	  -webkit-user-drag: none;
	}
/* Prevents IE11 from highlighting tiles in blue */
.leaflet-tile::selection {
	background: transparent;
}
/* Safari renders non-retina tile on retina better with this, but Chrome is worse */
.leaflet-safari .leaflet-tile {
	image-rendering: -webkit-optimize-contrast;
	}
/* hack that prevents hw layers "stretching" when loading new tiles */
.leaflet-safari .leaflet-tile-container {
	width: 1600px;
	height: 1600px;
	-webkit-transform-origin: 0 0;
	}
.leaflet-marker-icon,
.leaflet-marker-shadow {
	display: block;
	}
/* .leaflet-container svg: reset svg max-width decleration shipped in Joomla! (joomla.org) 3.x */
/* .leaflet-container img: map is broken in FF if you have max-width: 100% on tiles */
.leaflet-container .leaflet-overlay-pane svg {
	max-width: none !important;
	max-height: none !important;
	}
.leaflet-container .leaflet-marker-pane img,
.leaflet-container .leaflet-shadow-pane img,
.leaflet-container .leaflet-tile-pane img,
.leaflet-container img.leaflet-image-layer,
.leaflet-container .leaflet-tile {
	max-width: none !important;
	max-height: none !important;
	width: auto;
	padding: 0;
	}

.leaflet-container img.leaflet-tile {
	/* See: https://bugs.chromium.org/p/chromium/issues/detail?id=600120 */
	mix-blend-mode: plus-lighter;
}

.leaflet-container.leaflet-touch-zoom {
	-ms-touch-action: pan-x pan-y;
	touch-action: pan-x pan-y;
	}
.leaflet-container.leaflet-touch-drag {
	-ms-touch-action: pinch-zoom;
	/* Fallback for FF which doesn't support pinch-zoom */
	touch-action: none;
	touch-action: pinch-zoom;
}
.leaflet-container.leaflet-touch-drag.leaflet-touch-zoom {
	-ms-touch-action: none;
	touch-action: none;
}
.leaflet-container {
	-webkit-tap-highlight-color: transparent;
}
.leaflet-container a {
	-webkit-tap-highlight-color: rgba(51, 181, 229, 0.4);
}
.leaflet-tile {
	filter: inherit;
	visibility: hidden;
	}
.leaflet-tile-loaded {
	visibility: inherit;
	}
.leaflet-zoom-box {
	width: 0;
	height: 0;
	-moz-box-sizing: border-box;
	     box-sizing: border-box;
	z-index: 800;
	}
/* workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=888319 */
.leaflet-overlay-pane svg {
	-moz-user-select: none;
	}

.leaflet-pane         { z-index: 400; }

.leaflet-tile-pane    { z-index: 200; }
.leaflet-overlay-pane { z-index: 400; }
.leaflet-shadow-pane  { z-index: 500; }
.leaflet-marker-pane  { z-index: 600; }
.leaflet-tooltip-pane   { z-index: 650; }
.leaflet-popup-pane   { z-index: 700; }

.leaflet-map-pane canvas { z-index: 100; }
.leaflet-map-pane svg    { z-index: 200; }

.leaflet-vml-shape {
	width: 1px;
	height: 1px;
	}
.lvml {
	behavior: url(#default#VML);
	display: inline-block;
	position: absolute;
	}


/* control positioning */

.leaflet-control {
	position: relative;
	z-index: 800;
	pointer-events: visiblePainted; /* IE 9-10 doesn't have auto */
	pointer-events: auto;
	}
.leaflet-top,
.leaflet-bottom {
	position: absolute;
	z-index: 1000;
	pointer-events: none;
	}
.leaflet-top {
	top: 0;
	}
.leaflet-right {
	right: 0;
	}
.leaflet-bottom {
	bottom: 0;
	}
.leaflet-left {
	left: 0;
	}
.leaflet-control {
	float: left;
	clear: both;
	}
.leaflet-right .leaflet-control {
	float: right;
	}
.leaflet-top .leaflet-control {
	margin-top: 10px;
	}
.leaflet-bottom .leaflet-control {
	margin-bottom: 10px;
	}
.leaflet-left .leaflet-control {
	margin-left: 10px;
	}
.leaflet-right .leaflet-control {
	margin-right: 10px;
	}


/* zoom and fade animations */

.leaflet-fade-anim .leaflet-popup {
	opacity: 0;
	-webkit-transition: opacity 0.2s linear;
	   -moz-transition: opacity 0.2s linear;
	        transition: opacity 0.2s linear;
	}
.leaflet-fade-anim .leaflet-map-pane .leaflet-popup {
	opacity: 1;
	}
.leaflet-zoom-animated {
	-webkit-transform-origin: 0 0;
	    -ms-transform-origin: 0 0;
	        transform-origin: 0 0;
	}
svg.leaflet-zoom-animated {
	will-change: transform;
}

.leaflet-zoom-anim .leaflet-zoom-animated {
	-webkit-transition: -webkit-transform 0.25s cubic-bezier(0,0,0.25,1);
	   -moz-transition:    -moz-transform 0.25s cubic-bezier(0,0,0.25,1);
	        transition:         transform 0.25s cubic-bezier(0,0,0.25,1);
	}
.leaflet-zoom-anim .leaflet-tile,
.leaflet-pan-anim .leaflet-tile {
	-webkit-transition: none;
	   -moz-transition: none;
	        transition: none;
	}

.leaflet-zoom-anim .leaflet-zoom-hide {
	visibility: hidden;
	}


/* cursors */

.leaflet-interactive {
	cursor: pointer;
	}
.leaflet-grab {
	cursor: -webkit-grab;
	cursor:    -moz-grab;
	cursor:         grab;
	}
.leaflet-crosshair,
.leaflet-crosshair .leaflet-interactive {
	cursor: crosshair;
	}
.leaflet-popup-pane,
.leaflet-control {
	cursor: auto;
	}
.leaflet-dragging .leaflet-grab,
.leaflet-dragging .leaflet-grab .leaflet-interactive,
.leaflet-dragging .leaflet-marker-draggable {
	cursor: move;
	cursor: -webkit-grabbing;
	cursor:    -moz-grabbing;
	cursor:         grabbing;
	}

/* marker & overlays interactivity */
.leaflet-marker-icon,
.leaflet-marker-shadow,
.leaflet-image-layer,
.leaflet-pane > svg path,
.leaflet-tile-container {
	pointer-events: none;
	}

.leaflet-marker-icon.leaflet-interactive,
.leaflet-image-layer.leaflet-interactive,
.leaflet-pane > svg path.leaflet-interactive,
svg.leaflet-image-layer.leaflet-interactive path {
	pointer-events: visiblePainted; /* IE 9-10 doesn't have auto */
	pointer-events: auto;
	}

/* visual tweaks */

.leaflet-container {
	background: #ddd;
	outline-offset: 1px;
	}
.leaflet-container a {
	color: #0078A8;
	}
.leaflet-zoom-box {
	border: 2px dotted #38f;
	background: rgba(255,255,255,0.5);
	}


/* general typography */
.leaflet-container {
	font-family: "Helvetica Neue", Arial, Helvetica, sans-serif;
	font-size: 12px;
	font-size: 0.75rem;
	line-height: 1.5;
	}


/* general toolbar styles */

.leaflet-bar {
	box-shadow: 0 1px 5px rgba(0,0,0,0.65);
	border-radius: 4px;
	}
.leaflet-bar a {
	background-color: #fff;
	border-bottom: 1px solid #ccc;
	width: 26px;
	height: 26px;
	line-height: 26px;
	display: block;
	text-align: center;
	text-decoration: none;
	color: black;
	}
.leaflet-bar a,
.leaflet-control-layers-toggle {
	background-position: 50% 50%;
	background-repeat: no-repeat;
	display: block;
	}
.leaflet-bar a:hover,
.leaflet-bar a:focus {
	background-color: #f4f4f4;
	}
.leaflet-bar a:first-child {
	border-top-left-radius: 4px;
	border-top-right-radius: 4px;
	}
.leaflet-bar a:last-child {
	border-bottom-left-radius: 4px;
	border-bottom-right-radius: 4px;
	border-bottom: none;
	}
.leaflet-bar a.leaflet-disabled {
	cursor: default;
	background-color: #f4f4f4;
	color: #bbb;
	}

.leaflet-touch .leaflet-bar a {
	width: 30px;
	height: 30px;
	line-height: 30px;
	}
.leaflet-touch .leaflet-bar a:first-child {
	border-top-left-radius: 2px;
	border-top-right-radius: 2px;
	}
.leaflet-touch .leaflet-bar a:last-child {
	border-bottom-left-radius: 2px;
	border-bottom-right-radius: 2px;
	}

/* zoom control */

.leaflet-control-zoom-in,
.leaflet-control-zoom-out {
	font: bold 18px 'Lucida Console', Monaco, monospace;
	text-indent: 1px;
	}

.leaflet-touch .leaflet-control-zoom-in, .leaflet-touch .leaflet-control-zoom-out  {
	font-size: 22px;
	}


/* layers control */

.leaflet-control-layers {
	box-shadow: 0 1px 5px rgba(0,0,0,0.4);
	background: #fff;
	border-radius: 5px;
	}
.leaflet-control-layers-toggle {
	background-image: url(images/layers.png);
	width: 36px;
	height: 36px;
	}
.leaflet-retina .leaflet-control-layers-toggle {
	background-image: url(images/layers-2x.png);
	background-size: 26px 26px;
	}
.leaflet-touch .leaflet-control-layers-toggle {
	width: 44px;
	height: 44px;
	}
.leaflet-control-layers .leaflet-control-layers-list,
.leaflet-control-layers-expanded .leaflet-control-layers-toggle {
	display: none;
	}
.leaflet-control-layers-expanded .leaflet-control-layers-list {
	display: block;
	position: relative;
	}
.leaflet-control-layers-expanded {
	padding: 6px 10px 6px 6px;
	color: #333;
	background: #fff;
	}
.leaflet-control-layers-scrollbar {
	overflow-y: scroll;
	overflow-x: hidden;
	padding-right: 5px;
	}
.leaflet-control-layers-selector {
	margin-top: 2px;
	position: relative;
	top: 1px;
	}
.leaflet-control-layers label {
	display: block;
	font-size: 13px;
	font-size: 1.08333em;
	}
.leaflet-control-layers-separator {
	height: 0;
	border-top: 1px solid #ddd;
	margin: 5px -10px 5px -6px;
	}

/* Default icon URLs */
.leaflet-default-icon-path { /* used only in path-guessing heuristic, see L.Icon.Default */
	background-image: url(images/marker-icon.png);
	}


/* attribution and scale controls */

.leaflet-container .leaflet-control-attribution {
	background: #fff;
	background: rgba(255, 255, 255, 0.8);
	margin: 0;
	}
.leaflet-control-attribution,
.leaflet-control-scale-line {
	padding: 0 5px;
	color: #333;
	line-height: 1.4;
	}
.leaflet-control-attribution a {
	text-decoration: none;
	}
.leaflet-control-attribution a:hover,
.leaflet-control-attribution a:focus {
	text-decoration: underline;
	}
.leaflet-attribution-flag {
	display: inline !important;
	vertical-align: baseline !important;
	width: 1em;
	height: 0.6669em;
	}
.leaflet-left .leaflet-control-scale {
	margin-left: 5px;
	}
.leaflet-bottom .leaflet-control-scale {
	margin-bottom: 5px;
	}
.leaflet-control-scale-line {
	border: 2px solid #777;
	border-top: none;
	line-height: 1.1;
	padding: 2px 5px 1px;
	white-space: nowrap;
	-moz-box-sizing: border-box;
	     box-sizing: border-box;
	background: rgba(255, 255, 255, 0.8);
	text-shadow: 1px 1px #fff;
	}
.leaflet-control-scale-line:not(:first-child) {
	border-top: 2px solid #777;
	border-bottom: none;
	margin-top: -2px;
	}
.leaflet-control-scale-line:not(:first-child):not(:last-child) {
	border-bottom: 2px solid #777;
	}

.leaflet-touch .leaflet-control-attribution,
.leaflet-touch .leaflet-control-layers,
.leaflet-touch .leaflet-bar {
	box-shadow: none;
	}
.leaflet-touch .leaflet-control-layers,
.leaflet-touch .leaflet-bar {
	border: 2px solid rgba(0,0,0,0.2);
	background-clip: padding-box;
	}


/* popup */

.leaflet-popup {
	position: absolute;
	text-align: center;
	margin-bottom: 20px;
	}
.leaflet-popup-content-wrapper {
	padding: 1px;
	text-align: left;
	border-radius: 12px;
	}
.leaflet-popup-content {
	margin: 13px 24px 13px 20px;
	line-height: 1.3;
	font-size: 13px;
	font-size: 1.08333em;
	min-height: 1px;
	}
.leaflet-popup-content p {
	margin: 17px 0;
	margin: 1.3em 0;
	}
.leaflet-popup-tip-container {
	width: 40px;
	height: 20px;
	position: absolute;
	left: 50%;
	margin-top: -1px;
	margin-left: -20px;
	overflow: hidden;
	pointer-events: none;
	}
.leaflet-popup-tip {
	width: 17px;
	height: 17px;
	padding: 1px;

	margin: -10px auto 0;
	pointer-events: auto;

	-webkit-transform: rotate(45deg);
	   -moz-transform: rotate(45deg);
	    -ms-transform: rotate(45deg);
	        transform: rotate(45deg);
	}
.leaflet-popup-content-wrapper,
.leaflet-popup-tip {
	background: white;
	color: #333;
	box-shadow: 0 3px 14px rgba(0,0,0,0.4);
	}
.leaflet-container a.leaflet-popup-close-button {
	position: absolute;
	top: 0;
	right: 0;
	border: none;
	text-align: center;
	width: 24px;
	height: 24px;
	font: 16px/24px Tahoma, Verdana, sans-serif;
	color: #757575;
	text-decoration: none;
	background: transparent;
	}
.leaflet-container a.leaflet-popup-close-button:hover,
.leaflet-container a.leaflet-popup-close-button:focus {
	color: #585858;
	}
.leaflet-popup-scrolled {
	overflow: auto;
	}

.leaflet-oldie .leaflet-popup-content-wrapper {
	-ms-zoom: 1;
	}
.leaflet-oldie .leaflet-popup-tip {
	width: 24px;
	margin: 0 auto;

	-ms-filter: "progid:DXImageTransform.Microsoft.Matrix(M11=0.70710678, M12=0.70710678, M21=-0.70710678, M22=0.70710678)";
	filter: progid:DXImageTransform.Microsoft.Matrix(M11=0.70710678, M12=0.70710678, M21=-0.70710678, M22=0.70710678);
	}

.leaflet-oldie .leaflet-control-zoom,
.leaflet-oldie .leaflet-control-layers,
.leaflet-oldie .leaflet-popup-content-wrapper,
.leaflet-oldie .leaflet-popup-tip {
	border: 1px solid #999;
	}


/* div icon */

.leaflet-div-icon {
	background: #fff;
	border: 1px solid #666;
	}


/* Tooltip */
/* Base styles for the element that has a tooltip */
.leaflet-tooltip {
	position: absolute;
	padding: 6px;
	background-color: #fff;
	border: 1px solid #fff;
	border-radius: 3px;
	color: #222;
	white-space: nowrap;
	-webkit-user-select: none;
	-moz-user-select: none;
	-ms-user-select: none;
	user-select: none;
	pointer-events: none;
	box-shadow: 0 1px 3px rgba(0,0,0,0.4);
	}
.leaflet-tooltip.leaflet-interactive {
	cursor: pointer;
	pointer-events: auto;
	}
.leaflet-tooltip-top:before,
.leaflet-tooltip-bottom:before,
.leaflet-tooltip-left:before,
.leaflet-tooltip-right:before {
	position: absolute;
	pointer-events: none;
	border: 6px solid transparent;
	background: transparent;
	content: "";
	}

/* Directions */

.leaflet-tooltip-bottom {
	margin-top: 6px;
}
.leaflet-tooltip-top {
	margin-top: -6px;
}
.leaflet-tooltip-bottom:before,
.leaflet-tooltip-top:before {
	left: 50%;
	margin-left: -6px;
	}
.leaflet-tooltip-top:before {
	bottom: 0;
	margin-bottom: -12px;
	border-top-color: #fff;
	}
.leaflet-tooltip-bottom:before {
	top: 0;
	margin-top: -12px;
	margin-left: -6px;
	border-bottom-color: #fff;
	}
.leaflet-tooltip-left {
	margin-left: -6px;
}
.leaflet-tooltip-right {
	margin-left: 6px;
}
.leaflet-tooltip-left:before,
.leaflet-tooltip-right:before {
	top: 50%;
	margin-top: -6px;
	}
.leaflet-tooltip-left:before {
	right: 0;
	margin-right: -12px;
	border-left-color: #fff;
	}
.leaflet-tooltip-right:before {
	left: 0;
	margin-left: -12px;
	border-right-color: #fff;
	}

/* Printing */

@media print {
	/* Prevent printers from removing background-images of controls. */
	.leaflet-control {
		-webkit-print-color-adjust: exact;
		print-color-adjust: exact;
		}
	}
</style>
</head>
<body>

<div class="hdr">
  <svg width="20" height="20" viewBox="0 0 24 24" fill="#03a9f4">
    <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z"/>
  </svg>
  <div class="hdr-title">Vessel Command</div>
  <div class="hdr-right">
    <a href="/settings" style="color:#607080;font-size:18px;text-decoration:none;line-height:1" title="Network Settings">&#9881;</a>
    <div id="mqtt-dot" title="MQTT status"></div>
    <span id="mqtt-txt" style="margin-right:6px">MQTT</span>
    <div id="conn-dot"></div>
    <span id="conn-txt">Connecting…</span>
  </div>
</div>

<div class="tabs">
  <div class="tab active" id="tab0" onclick="showTab(0)">
    <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" style="vertical-align:middle;margin-right:3px"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm.5-5H11v6l5.25 3.15.75-1.23-4.5-2.67V7z"/></svg>
    Nav
  </div>
  <div class="tab" id="tab1" onclick="showTab(1)">
    <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" style="vertical-align:middle;margin-right:3px"><path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg>
    Eng
  </div>
  <div class="tab" id="tab2" onclick="showTab(2)">
    <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" style="vertical-align:middle;margin-right:3px"><path d="M6.76 4.84l-1.8-1.79-1.41 1.41 1.79 1.79 1.42-1.41zM4 10.5H1v2h3v-2zm9-9.95h-2V3.5h2V.55zm7.45 3.91l-1.41-1.41-1.79 1.79 1.41 1.41 1.79-1.79zM20 10.5v2h3v-2h-3zm-8-5c-3.31 0-6 2.69-6 6s2.69 6 6 6 6-2.69 6-6-2.69-6-6-6zm-1 16.95h2V19.5h-2v2.95zm-7.45-3.91l1.41 1.41 1.79-1.8-1.41-1.41-1.79 1.8z"/></svg>
    Env
  </div>
  <div class="tab" id="tab3" onclick="showTab(3)">
    <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" style="vertical-align:middle;margin-right:3px"><path d="M12 22c1.1 0 2-.9 2-2h-4c0 1.1.9 2 2 2zm6-6v-5c0-3.07-1.64-5.64-4.5-6.32V4c0-.83-.67-1.5-1.5-1.5s-1.5.67-1.5 1.5v.68C7.63 5.36 6 7.92 6 11v5l-2 2v1h16v-1l-2-2z"/></svg>
    Alarms
  </div>
</div>

<div id="mock-strip">&#9655; Simulation mode &mdash; mock data active &mdash; not representative of real-world conditions &#9665;</div>

<div class="content">

<!-- ═══════════ TAB 0: NAVIGATION ═══════════ -->
<div class="panel active" id="p0">
  <div class="g2 gap">
    <div class="card">
      <div class="card-hdr">Position Fix</div>
      <div class="card-body">
        <div class="pos">
          <div>LAT <span id="lat" style="color:#03a9f4">--°--.--'N</span></div>
          <div>LON <span id="lon" style="color:#03a9f4">---°--.--'W</span></div>
        </div>
        <div style="margin-top:8px;font-size:10px;color:#9e9e9e" id="gps-time">GPS: --:--:-- UTC</div>
      </div>
    </div>
    <div class="card" style="padding:0">
      <div class="inst" style="height:100%;display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center">
        <span class="lbl" style="letter-spacing:2px">Depth Under Keel</span>
        <span class="val" style="font-size:52px;color:#0055FF" id="depth">--.-</span>
        <span class="unit" style="font-size:15px;color:#0055FF">METRES</span>
      </div>
    </div>
  </div>

  <div class="g2 gap">
    <div class="inst" style="text-align:center">
      <span class="lbl">Compass Heading</span>
      <span class="val" style="font-size:42px" id="hdg">---°</span>
      <span class="unit">MAGNETIC</span>
    </div>
    <div class="inst" style="text-align:center">
      <span class="lbl">Course Over Ground</span>
      <span class="val" style="font-size:42px" id="cog">---°</span>
      <span class="unit">TRUE BEARING</span>
    </div>
    <div class="inst" style="text-align:center">
      <span class="lbl">Speed Over Ground</span>
      <span class="val" style="font-size:42px;color:#00AA00" id="sog">-.-</span>
      <span class="unit">KNOTS (GPS)</span>
    </div>
    <div class="inst" style="text-align:center">
      <span class="lbl">Speed Through Water</span>
      <span class="val" style="font-size:42px" id="stw">-.-</span>
      <span class="unit">KNOTS (LOG)</span>
    </div>
  </div>

  <div class="g2">
    <div class="inst">
      <span class="lbl">Trip Log</span>
      <span class="val" style="font-size:26px" id="trip">-.--</span>
      <span class="unit">NAUTICAL MILES</span>
    </div>
    <div class="inst">
      <span class="lbl">Total Log</span>
      <span class="val" style="font-size:26px" id="log">-.--</span>
      <span class="unit">NAUTICAL MILES</span>
    </div>
  </div>

  <div class="card" style="padding:0;overflow:hidden;margin-top:10px">
    <div class="card-hdr">Chart — OpenSeaMap</div>
    <div id="chart"><div id="chart-msg">Awaiting GPS fix&hellip;</div></div>
    <div style="padding:3px 10px;font-size:9px;color:#666;background:#181f2e">Pre-cache tiles while connected &middot; Service Worker serves offline</div>
  </div>
</div>

<!-- ═══════════ TAB 1: ENGINEERING ═══════════ -->
<div class="panel" id="p1">
  <div class="g2">
    <div class="card">
      <div class="card-hdr">DC Power</div>
      <div class="card-body">
        <div class="bc">
          <div class="bc-name">Battery Voltage</div>
          <div class="bc-row">
            <div class="bc-track"><div class="bc-fill" id="batt-bar" style="width:0%;background:#00AA00"></div></div>
            <div class="bc-val" id="batt-v">--.- V</div>
          </div>
          <div class="bc-scale"><span>10V</span><span>11V</span><span>12V</span><span>13V</span><span>14V</span><span>15V</span></div>
          <div style="margin-top:6px;font-size:11px;font-weight:700" id="batt-status">● READING…</div>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="card-hdr">Refrigeration</div>
      <div class="card-body">
        <div class="bc">
          <div class="bc-name">Fridge Temperature</div>
          <div class="bc-row">
            <div class="bc-track"><div class="bc-fill" id="fridge-bar" style="width:0%;background:#00AA00"></div></div>
            <div class="bc-val" id="eng-fridge">--.- °C</div>
          </div>
          <div class="bc-scale"><span>-4°</span><span>0°</span><span>4°</span><span>8°</span><span>12°</span><span>16°</span></div>
          <div style="margin-top:6px;font-size:11px;font-weight:700" id="fridge-status">● READING…</div>
        </div>
      </div>
    </div>
  </div>

  <div class="hint">
    Engine instrumentation (RPM · Coolant · Oil Pressure · Exhaust) will appear here when sensors are added to LocalSensors.
  </div>
</div>

<!-- ═══════════ TAB 2: ENVIRONMENT ═══════════ -->
<div class="panel" id="p2">
  <div class="g2 gap">
    <div class="card" style="padding:12px;display:flex;flex-direction:column;align-items:center">
      <div style="font-size:10px;font-weight:600;text-transform:uppercase;color:#9e9e9e;letter-spacing:.5px;margin-bottom:8px">Apparent Wind Direction</div>
      <canvas id="windGauge" width="230" height="230"></canvas>
    </div>

    <div class="card" style="padding:0">
      <div class="inst" style="height:100%;padding:16px;display:flex;flex-direction:column;justify-content:center">
        <div style="font-size:13px;font-weight:800;text-transform:uppercase;border-bottom:3px solid #000;padding-bottom:8px;margin-bottom:12px;letter-spacing:1px">Wind Data</div>
        <div style="margin-bottom:12px">
          <div class="lbl">AWS — Apparent Velocity</div>
          <span class="val" style="font-size:28px;color:#0055FF" id="aws">--.- kn</span>
        </div>
        <div style="margin-bottom:12px">
          <div class="lbl">TWS — True Speed</div>
          <span class="val" style="font-size:28px" id="tws">--.- kn</span>
        </div>
        <div>
          <div class="lbl">TWA — True Angle</div>
          <span class="val" style="font-size:28px" id="twa">---°</span>
        </div>
      </div>
    </div>
  </div>

  <div class="card" style="padding:12px">
    <div class="card-hdr" style="padding:0 0 10px 0">Temperatures &amp; Helm</div>
    <div class="g3">
      <div class="tile" id="fridge-tile" style="background:#153a15;border:2px solid #00AA00">
        <div class="tile-name" style="color:#66FF66">Refrigerator</div>
        <div class="tile-val" id="fridge-t" style="color:#00DD00">--.-°C</div>
        <div class="tile-sub" id="fridge-t-sub" style="color:#00AA00">DS18B20</div>
      </div>
      <div class="tile" style="background:#15253a;border:2px solid #0055FF">
        <div class="tile-name" style="color:#6699FF">Sea Surface</div>
        <div class="tile-val" style="color:#4488FF" id="sea-t">--.-°C</div>
        <div class="tile-sub" style="color:#0055FF">NMEA 2000</div>
      </div>
      <div class="tile" style="background:#2a2a2a;border:2px solid #555">
        <div class="tile-name" style="color:#9e9e9e">Rudder Angle</div>
        <div class="tile-val" style="color:#ccc" id="rudder">---°</div>
        <div class="tile-sub" style="color:#555">NMEA 2000</div>
      </div>
    </div>
  </div>

  <div class="card" style="padding:12px;margin-top:10px">
    <div class="card-hdr" style="padding:0 0 10px 0">Ruuvi BLE Sensors</div>
    <div id="ruuvi-grid" class="g2" style="gap:8px"></div>
    <div id="ruuvi-empty" style="color:#5a6a7a;font-size:12px;text-align:center;padding:10px">No Ruuvi sensors detected yet</div>
  </div>
</div>

<!-- ═══════════ TAB 3: ALARMS ═══════════ -->
<div class="panel" id="p3">
  <div id="alarm-banner" style="display:none;margin-bottom:10px;padding:10px 14px;background:#2a1a1a;border-radius:8px;border-left:4px solid #CC0000">
    <span style="font-size:11px;font-weight:700;color:#FF6666;text-transform:uppercase;letter-spacing:.5px">⚠ ACTIVE ALARMS PRESENT</span>
  </div>
  <div style="padding:9px 12px;background:#1e2633;border-radius:8px;font-size:10px;color:#9e9e9e;margin-bottom:10px">
    Thresholds: Fridge &gt; 12°C &nbsp;·&nbsp; Battery &lt; 11V &nbsp;·&nbsp; Depth &lt; 2m
  </div>
  <div class="g3">
    <div class="card" style="padding:0;overflow:hidden" id="alm-fridge">
      <div class="alm-ok">
        <div class="alm-icon">🟢</div>
        <div class="alm-name">FRIDGE TEMP</div>
        <div class="alm-detail" id="alm-fridge-val">--.-°C</div>
      </div>
    </div>
    <div class="card" style="padding:0;overflow:hidden" id="alm-batt">
      <div class="alm-ok">
        <div class="alm-icon">🟢</div>
        <div class="alm-name">BATTERY</div>
        <div class="alm-detail" id="alm-batt-val">--.- V</div>
      </div>
    </div>
    <div class="card" style="padding:0;overflow:hidden" id="alm-depth">
      <div class="alm-ok">
        <div class="alm-icon">🟢</div>
        <div class="alm-name">WATER DEPTH</div>
        <div class="alm-detail" id="alm-depth-val">-.- m</div>
      </div>
    </div>
  </div>
</div>

</div><!-- /content -->

<script src="/leaflet.js"></script>
<script>
// ─── Tab switching ────────────────────────────────────────────────────────────
var tabs   = document.querySelectorAll('.tab');
var panels = document.querySelectorAll('.panel');
function showTab(i) {
  tabs.forEach(function(t,j)  { t.classList.toggle('active', j===i); });
  panels.forEach(function(p,j){ p.classList.toggle('active', j===i); });
  if (i===2) setTimeout(drawGauge, 60);
  if (i===0 && _map) setTimeout(function(){ _map.invalidateSize(); }, 100);
}

// ─── Formatting helpers ───────────────────────────────────────────────────────
function fmtLat(v) {
  if (v == null || isNaN(v)) return '--\xb0--.--\'N';
  var h = v >= 0 ? 'N' : 'S'; v = Math.abs(v);
  var d = Math.floor(v), m = (v - d) * 60;
  return d + '\xb0' + m.toFixed(2) + '\'' + h;
}
function fmtLon(v) {
  if (v == null || isNaN(v)) return '---\xb0--.--\'W';
  var h = v >= 0 ? 'E' : 'W'; v = Math.abs(v);
  var d = Math.floor(v), m = (v - d) * 60;
  return (d < 100 ? '0' : '') + d + '\xb0' + m.toFixed(2) + '\'' + h;
}
function fmtTime(s) {
  if (!s) return '--:--:-- UTC';
  var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), ss = Math.floor(s % 60);
  return ('0'+h).slice(-2) + ':' + ('0'+m).slice(-2) + ':' + ('0'+ss).slice(-2) + ' UTC';
}
function fmtDeg(v, dp) { return (v==null||isNaN(v)) ? '---\xb0' : v.toFixed(dp||0)+'\xb0'; }
function fmtKn(v)      { return (v==null||isNaN(v)) ? '--.- kn'  : v.toFixed(1)+' kn'; }
function fmtM(v)       { return (v==null||isNaN(v)) ? '--.-'     : v.toFixed(1); }
function fmtC(v)       { return (v==null||isNaN(v)) ? '--.-\xb0C': v.toFixed(1)+'\xb0C'; }
function fmtV(v)       { return (v==null||isNaN(v)) ? '--.- V'   : v.toFixed(2)+' V'; }
function setText(id, s) { var el=document.getElementById(id); if(el) el.textContent=s; }

// ─── Nautical chart (Leaflet + OpenSeaMap) ────────────────────────────────────
var _map = null, _vesselMk = null;
function _vesselIcon(hdg) {
  var svg = '<svg xmlns="http://www.w3.org/2000/svg" width="20" height="36" viewBox="0 0 20 36">'
          + '<polygon points="10,1 1,35 10,27 19,35" fill="#e03030" stroke="#7a0000" stroke-width="1.5"/>'
          + '</svg>';
  return L.divIcon({
    html: '<div style="transform:rotate('+hdg+'deg);transform-origin:10px 18px;line-height:0">'+svg+'</div>',
    iconSize:[20,36], iconAnchor:[10,18], className:''
  });
}
function _initMap(lat, lon) {
  if (typeof L === 'undefined') return;
  var msgEl = document.getElementById('chart-msg');
  if (msgEl) msgEl.style.display = 'none';
  _map = L.map('chart', {zoomControl:true}).setView([lat, lon], 13);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution:'&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a>',
    maxZoom:18
  }).addTo(_map);
  L.tileLayer('https://tiles.openseamap.org/seamark/{z}/{x}/{y}.png', {
    attribution:'&copy; <a href="http://www.openseamap.org">OpenSeaMap</a>',
    maxZoom:18
  }).addTo(_map);
  _vesselMk = L.marker([lat, lon], {icon:_vesselIcon(0)}).addTo(_map);
}
function updateMap(lat, lon, hdg) {
  if (!lat || !lon) return;
  if (!_map) { _initMap(lat, lon); return; }
  _vesselMk.setLatLng([lat, lon]);
  _vesselMk.setIcon(_vesselIcon(hdg || 0));
  if (_map.distance(_map.getCenter(), [lat, lon]) > 150) _map.panTo([lat, lon]);
}

// ─── Wind gauge (canvas) ──────────────────────────────────────────────────────
var _awa = 0;
function drawGauge() {
  var c = document.getElementById('windGauge');
  if (!c) return;
  var ctx = c.getContext('2d');
  var cx = 115, cy = 115, R = 108;
  ctx.clearRect(0, 0, 230, 230);
  if (_awa === null) {
    ctx.beginPath(); ctx.arc(cx, cy, R, 0, 2*Math.PI);
    ctx.fillStyle = '#1e2633'; ctx.fill();
    ctx.strokeStyle = '#333'; ctx.lineWidth = 4; ctx.stroke();
    ctx.fillStyle = '#FF4444'; ctx.font = 'bold 14px monospace';
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText('⚠ NO DATA', cx, cy);
    return;
  }

  // White face + border
  ctx.beginPath(); ctx.arc(cx, cy, R, 0, 2*Math.PI);
  ctx.fillStyle = '#fff'; ctx.fill();
  ctx.strokeStyle = '#000'; ctx.lineWidth = 4; ctx.stroke();

  // Tick marks (every 5°)
  for (var i = 0; i < 72; i++) {
    var a   = (i * 5 - 90) * Math.PI / 180;
    var big = (i % 9 === 0), med = (i % 3 === 0);
    var len = big ? 22 : med ? 13 : 7;
    ctx.beginPath();
    ctx.moveTo(cx + (R-4)*Math.cos(a), cy + (R-4)*Math.sin(a));
    ctx.lineTo(cx + (R-4-len)*Math.cos(a), cy + (R-4-len)*Math.sin(a));
    ctx.strokeStyle = '#000';
    ctx.lineWidth = big ? 2.5 : med ? 1.2 : 0.7;
    ctx.stroke();
  }

  // Cardinal and inter-cardinal labels
  var lbls = [['N',-90,true],['045',-45,false],['E',0,true],['135',45,false],
              ['S',90,true],['225',135,false],['W',180,true],['315',-135,false]];
  lbls.forEach(function(l) {
    var rad = l[1] * Math.PI / 180;
    var d = R - 42;
    ctx.fillStyle = '#000';
    ctx.font = (l[2] ? 'bold 13px' : '10px') + ' sans-serif';
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(l[0], cx + d*Math.cos(rad), cy + d*Math.sin(rad));
  });

  // PORT / STBD labels
  ctx.font = 'bold 9px sans-serif';
  ctx.fillStyle = '#CC0000'; ctx.fillText('PORT', cx - 44, cy + R - 30);
  ctx.fillStyle = '#006600'; ctx.fillText('STBD', cx + 36, cy + R - 30);

  // Value readout box
  ctx.fillStyle = '#f5f5f5'; ctx.fillRect(cx - 32, cy + 12, 64, 28);
  ctx.strokeStyle = '#000'; ctx.lineWidth = 1.5; ctx.strokeRect(cx - 32, cy + 12, 64, 28);
  ctx.fillStyle = '#000'; ctx.font = 'bold 17px monospace';
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(Math.round(_awa) + '\xb0', cx, cy + 26);
  ctx.font = '8px sans-serif'; ctx.fillStyle = '#666';
  ctx.fillText('APPARENT', cx, cy + 46);

  // Needle
  var nr = (_awa - 90) * Math.PI / 180;
  ctx.save(); ctx.translate(cx, cy); ctx.rotate(nr + Math.PI/2);
  ctx.beginPath();
  ctx.moveTo(0, -(R-18)); ctx.lineTo(-5, 14); ctx.lineTo(0, 8); ctx.lineTo(5, 14);
  ctx.closePath();
  ctx.fillStyle = '#FF0000'; ctx.fill();
  ctx.strokeStyle = '#880000'; ctx.lineWidth = 1; ctx.stroke();
  // Counter-weight
  ctx.beginPath();
  ctx.moveTo(0, 8); ctx.lineTo(-4, 22); ctx.lineTo(0, 26); ctx.lineTo(4, 22);
  ctx.closePath(); ctx.fillStyle = '#333'; ctx.fill();
  ctx.restore();

  // Centre hub
  ctx.beginPath(); ctx.arc(cx, cy, 7, 0, 2*Math.PI);
  ctx.fillStyle = '#222'; ctx.fill();
  ctx.strokeStyle = '#fff'; ctx.lineWidth = 1.5; ctx.stroke();
}

// ─── DOM update from /data payload ───────────────────────────────────────────
function update(d) {
  // ── Data-source mode ──────────────────────────────────────────────────────
  var isMock = (d.dataSource === 'mock');
  var gpsOK  = isMock || !!d.gpsValid;
  var windOK = isMock || !!d.windValid;
  var dptOK  = isMock || !!d.depthValid;
  document.body.classList.toggle('mock-mode', isMock);

  // setV: sets value text, or injects a vivid NO DATA chip when the sensor
  // group is offline in live mode.
  function setV(id, str, ok) {
    var el = document.getElementById(id);
    if (!el) return;
    if (!ok) { el.innerHTML = '<span class="nd">⚠ NO DATA</span>'; }
    else     { el.textContent = str; }
  }

  // Navigation
  setV('lat',      fmtLat(d.Latitude),                          gpsOK);
  setV('lon',      fmtLon(d.Longitude),                         gpsOK);
  setText('gps-time', 'GPS: ' + fmtTime(d.GPSTime));
  setV('hdg',      fmtDeg(d.Heading),                           gpsOK);
  setV('cog',      fmtDeg(d.COG),                               gpsOK);
  setV('sog',      d.SOG != null ? d.SOG.toFixed(1) : '--.-',   gpsOK);
  setV('stw',      d.STW != null ? d.STW.toFixed(1) : '--.-',   gpsOK);
  setV('depth',    fmtM(d.WaterDepth),                          dptOK);
  setV('trip',     d.TripLog != null ? d.TripLog.toFixed(2) : '-.--', gpsOK);
  setV('log',      d.Log     != null ? d.Log.toFixed(2)     : '-.--', gpsOK);
  if (gpsOK) updateMap(d.Latitude, d.Longitude, d.Heading);

  // Engineering — battery
  var bv = d.BatteryVoltage;
  setText('batt-v', fmtV(bv));
  var bpct = bv != null ? Math.max(0, Math.min(100, (bv - 10) / 5 * 100)) : 0;
  var bbar = document.getElementById('batt-bar');
  if (bbar) {
    bbar.style.width = bpct + '%';
    bbar.style.background = bv < 11 ? '#CC0000' : bv < 12.5 ? '#FF8800' : '#00AA00';
  }
  var bstatus = bv == null ? '● READING…'
              : bv < 11   ? '▼ LOW VOLTAGE'
              : bv > 13.8 ? '▲ CHARGING'
              :              '● DISCHARGING';
  var bcolor  = bv == null ? '#999' : bv < 11 ? '#CC0000' : bv > 13.8 ? '#00AA00' : '#FF8800';
  var bsel = document.getElementById('batt-status');
  if (bsel) { bsel.textContent = bstatus; bsel.style.color = bcolor; }

  // Engineering — fridge
  var ft = d.FridgeTemperature;
  setText('eng-fridge', fmtC(ft));
  var fpct = ft != null ? Math.max(0, Math.min(100, (ft + 4) / 20 * 100)) : 0;
  var fbar = document.getElementById('fridge-bar');
  if (fbar) {
    fbar.style.width = fpct + '%';
    fbar.style.background = ft > 12 ? '#CC0000' : ft > 6 ? '#FF8800' : '#00AA00';
  }
  var fstatus = ft == null ? '● READING…' : ft > 12 ? '⚠ OVERTEMP' : ft > 6 ? '! WARM' : '✓ OK';
  var fcolor  = ft == null ? '#999' : ft > 12 ? '#CC0000' : ft > 6 ? '#FF8800' : '#00AA00';
  var fsel = document.getElementById('fridge-status');
  if (fsel) { fsel.textContent = fstatus; fsel.style.color = fcolor; }

  // Environment — wind
  _awa = windOK ? (d.AWA || 0) : null;
  setV('aws', fmtKn(d.AWS),    windOK);
  setV('tws', fmtKn(d.TWS),    windOK);
  setV('twa', fmtDeg(d.TWA),   windOK);
  drawGauge();

  // Environment — temps & rudder
  setV('sea-t',  fmtC(d.WaterTemperature),      gpsOK);
  setV('rudder', fmtDeg(d.RudderPosition, 1),   gpsOK);

  // Fridge tile styling
  var ftile = document.getElementById('fridge-tile');
  setText('fridge-t', fmtC(ft));
  if (ftile) {
    if (ft > 12) {
      ftile.style.background = '#3a1515'; ftile.style.border = '2px solid #CC0000';
      document.getElementById('fridge-t').style.color = '#FF4444';
      setText('fridge-t-sub', '⚠ OVERTEMP');
      document.getElementById('fridge-t-sub').style.color = '#CC0000';
    } else {
      ftile.style.background = '#153a15'; ftile.style.border = '2px solid #00AA00';
      document.getElementById('fridge-t').style.color = '#00DD00';
      setText('fridge-t-sub', '✓ NORMAL');
      document.getElementById('fridge-t-sub').style.color = '#00AA00';
    }
  }

  // Alarms
  var active = 0;
  function alarmCard(id, valId, alarm, noData, label, valStr, limit) {
    var el = document.getElementById(id);
    if (!el) return;
    if (noData) {
      el.innerHTML = '<div style="background:#111;border:3px solid #2a2a2a;padding:16px 10px;text-align:center">'
                   + '<div class="alm-icon" style="color:#333">&#9744;</div>'
                   + '<div class="alm-name" style="color:#444">'+label+'</div>'
                   + '<div class="alm-detail" style="color:#333">NO LIVE DATA</div></div>';
      return;
    }
    if (alarm) active++;
    el.innerHTML = alarm
      ? '<div class="alm-act"><div class="alm-icon">🚨</div><div class="alm-name">'+label+'</div><div class="alm-detail">'+valStr+' — LIMIT '+limit+'</div></div>'
      : '<div class="alm-ok"><div class="alm-icon">🟢</div><div class="alm-name">'+label+' OK</div><div class="alm-detail">'+valStr+'</div></div>';
  }
  var localOK = isMock || !!d.localValid;
  alarmCard('alm-fridge', 'alm-fridge-val', ft != null && ft > 12,  !localOK, 'FRIDGE TEMP', fmtC(ft),          '12\xb0C');
  alarmCard('alm-batt',   'alm-batt-val',   bv != null && bv < 11,  !localOK, 'BATTERY',     fmtV(bv),          '11V');
  alarmCard('alm-depth',  'alm-depth-val',  d.WaterDepth != null && d.WaterDepth > 0.1 && d.WaterDepth < 2,
                                                                     !dptOK,   'DEPTH',        fmtM(d.WaterDepth)+'m', '2m');
  var banner = document.getElementById('alarm-banner');
  if (banner) banner.style.display = active > 0 ? 'block' : 'none';

  // Ruuvi BLE sensors
  var grid  = document.getElementById('ruuvi-grid');
  var empty = document.getElementById('ruuvi-empty');
  if (grid && d.ruuvi) {
    var any = false;
    grid.innerHTML = '';
    for (var ri = 0; ri < d.ruuvi.length; ri++) {
      var sr = d.ruuvi[ri];
      if (!sr.active || sr.stale) continue;
      any = true;
      var lbl  = sr.label || ('Sensor ' + (ri + 1));
      var tval = sr.temp  != null ? sr.temp.toFixed(1)  + '\xb0C' : '--.-\xb0C';
      var envParts = [];
      if (sr.hum  != null) envParts.push(sr.hum.toFixed(1)  + '% RH');
      if (sr.pres != null) envParts.push(sr.pres.toFixed(0) + ' hPa');
      var motParts = [];
      if (sr.heel  != null) motParts.push('H:' + sr.heel.toFixed(1)  + '\xb0');
      if (sr.pitch != null) motParts.push('P:' + sr.pitch.toFixed(1) + '\xb0');
      if (sr.movement != null) motParts.push('⇳' + sr.movement);
      var hwParts = [];
      if (sr.batt != null) hwParts.push((sr.batt / 1000).toFixed(2) + 'V');
      if (sr.rssi != null) hwParts.push(sr.rssi + ' dBm');
      var div = document.createElement('div');
      div.className = 'tile';
      div.style.cssText = 'background:#0c2630;border:2px solid #00838f';
      div.innerHTML = '<div class="tile-name" style="color:#4dd0e1">' + lbl + '</div>'
                    + '<div class="tile-val" style="color:#00e5ff">' + tval + '</div>'
                    + (envParts.length ? '<div class="tile-sub" style="color:#00838f">' + envParts.join(' \xb7 ') + '</div>' : '')
                    + (motParts.length ? '<div class="tile-sub" style="color:#80cbc4;margin-top:3px">' + motParts.join(' \xb7 ') + '</div>' : '')
                    + (hwParts.length  ? '<div class="tile-sub" style="color:#546e7a;margin-top:2px">' + hwParts.join(' \xb7 ')  + '</div>' : '');
      grid.appendChild(div);
    }
    if (empty) empty.style.display = any ? 'none' : 'block';
  }
}

// ─── Fetch /data every second ─────────────────────────────────────────────────
var _lastOk = 0;
function poll() {
  fetch('/data')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      _lastOk = Date.now();
      document.getElementById('conn-dot').className = 'ok';
      setText('conn-txt', 'Live');
      update(d);
    })
    .catch(function() {
      if (Date.now() - _lastOk > 5000) {
        document.getElementById('conn-dot').className = '';
        setText('conn-txt', 'No signal');
      }
    });
}

function pollMqtt() {
  fetch('/api/mqtt/config')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      var dot = document.getElementById('mqtt-dot');
      var txt = document.getElementById('mqtt-txt');
      if (!d.enabled) { dot.className=''; txt.textContent='MQTT off'; }
      else if (d.connected) { dot.className='ok'; txt.textContent='MQTT'; }
      else { dot.className='warn'; txt.textContent='MQTT'; }
    })
    .catch(function() {});
}

poll();
setInterval(poll, 1000);
pollMqtt();
setInterval(pollMqtt, 30000);
setTimeout(drawGauge, 150);

if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('/sw.js').catch(function(){});
}
</script>
</body>
</html>
)=====";
