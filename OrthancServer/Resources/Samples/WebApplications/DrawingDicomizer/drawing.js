/**
 * Copyright 2010 William Malone (www.williammalone.com)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/


/**
 * This code comes from the blog entry "Create a Drawing App with
 * HTML5 Canvas and JavaScript" by William Malone. It is the "simple
 * demo" of a pure HTML5 drawing application.
 *
 * http://www.williammalone.com/articles/create-html5-canvas-javascript-drawing-app/
 *
 * To keep this sample code as simple as possible, we do not implement
 * hacks for the canvas of Microsoft Internet Explorer.
 **/


if ($.browser.msie) {
  alert('Please use Mozilla Firefox or Google Chrome. Microsoft Internet Explorer is not supported.');
}


var context;
var clickX = new Array();
var clickY = new Array();
var clickDrag = new Array();
var paint;


function addClick(x, y, dragging)
{
  clickX.push(x);
  clickY.push(y);
  clickDrag.push(dragging);
}


function Redraw() 
{
  context.fillStyle = '#ffffff';
  context.fillRect(0, 0, context.canvas.width, context.canvas.height); // Clears the canvas
  
  context.strokeStyle = '#df4b26';
  context.lineJoin = 'round';
  context.lineWidth = 5;
  
  for (var i=0; i < clickX.length; i++) {		
    context.beginPath();
    if (clickDrag[i] && i) {
      context.moveTo(clickX[i - 1], clickY[i - 1]);
    } else {
      context.moveTo(clickX[i] - 1, clickY[i]);
    }
    context.lineTo(clickX[i], clickY[i]);
    context.closePath();
    context.stroke();
  }
}


$(document).ready(function() {
  context = document.getElementById('canvas').getContext('2d');
  Redraw();

  $('#canvas').mousedown(function(e) {
    var mouseX = e.pageX - this.offsetLeft;
    var mouseY = e.pageY - this.offsetTop;
    
    paint = true;
    addClick(e.pageX - this.offsetLeft, e.pageY - this.offsetTop);
    Redraw();
  });

  $('#canvas').mousemove(function(e) {
    if(paint) {
      addClick(e.pageX - this.offsetLeft, e.pageY - this.offsetTop, true);
      Redraw();
    }
  });

  $('#canvas').mouseup(function(e) {
    paint = false;
  });

  $('#canvas').mouseleave(function(e) {
    paint = false;
  });
});
