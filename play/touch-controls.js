// touch-controls: on-screen controls and page layout for pixel-art web games.
// Kept in https://github.com/eralpkaraduman/kurban-bayrami (port/web/).
// Time Escape downloads this file from there at a pinned commit.
//
// One call sets up the page around a game canvas:
//
//   var controls = TouchControls.setup({
//     canvas: document.getElementById('canvas'),
//     frame: document.getElementById('frame'),   // the canvas's parent; the bar goes under the canvas
//     dpad: { left: 263, right: 262, up: 265, down: 264 },  // key codes; leave a direction out to hide it
//     buttons: [ { label: 'X', caption: 'Jump', key: 88 }, ... ],
//     onKey: function (key, down) { ... },       // called when a button goes down or up
//   });
//   var Module = { canvas: canvas, postRun: [controls.fit] };  // fit once the game has sized its canvas
//
// On touch screens it adds a control bar under the game, fits the game to
// the full width above it, and locks the page against zooming and scrolling.
// Elsewhere it scales the game by a whole number of physical pixels, so the
// pixel art stays sharp, as long as that gives at least 2x. Add ?touch=1 to
// the URL to force the touch layout.
(function () {
  'use strict';

  function shouldShow() {
    if (/[?&]touch=1\b/.test(location.search)) return true;
    if (/[?&]touch=0\b/.test(location.search)) return false;
    return window.matchMedia('(pointer: coarse)').matches;
  }

  function el(tag, className, parent) {
    var node = document.createElement(tag);
    node.className = className;
    if (parent) parent.appendChild(node);
    return node;
  }

  function addButton(parent, className, key, label, caption) {
    var button = el('div', 'tc-button ' + className, parent);
    button.dataset.key = String(key);
    button.setAttribute('role', 'button');
    button.setAttribute('aria-label', caption || label);
    el('span', 'tc-label', button).textContent = label;
    if (caption) el('span', 'tc-caption', button).textContent = caption;
    return button;
  }

  // Builds the bar inside host. Several fingers work at once, and a finger
  // can slide from one button to another without lifting.
  function attach(host, options) {
    host.classList.add('tc-console');
    var bar = el('div', 'tc-bar', host);
    var dpadKeys = options.dpad || {};
    var dpad = el('div', 'tc-dpad', bar);
    // Without up and down, the D-pad is a single compact row.
    if (dpadKeys.up == null && dpadKeys.down == null) dpad.classList.add('tc-dpad-row');
    var arrows = { up: '▲', left: '◀', right: '▶', down: '▼' };
    Object.keys(arrows).forEach(function (dir) {
      if (dpadKeys[dir] != null) addButton(dpad, 'tc-' + dir, dpadKeys[dir], arrows[dir], dir);
    });
    var actions = el('div', 'tc-actions', bar);
    (options.buttons || []).forEach(function (b) {
      addButton(actions, 'tc-round', b.key, b.label, b.caption);
    });

    // Which key each finger is on, and how many fingers hold each key.
    var fingers = new Map();
    var holds = new Map();

    function keyAt(x, y) {
      var target = document.elementFromPoint(x, y);
      var button = target && target.closest('[data-key]');
      return button && bar.contains(button) ? Number(button.dataset.key) : null;
    }

    function setPressed(key, down) {
      bar.querySelectorAll('[data-key="' + key + '"]').forEach(function (button) {
        button.classList.toggle('tc-pressed', down);
      });
      options.onKey(key, down);
    }

    function move(id, key) {
      var old = fingers.has(id) ? fingers.get(id) : null;
      if (old === key) return;
      if (old !== null) {
        var left = holds.get(old) - 1;
        holds.set(old, left);
        if (left === 0) setPressed(old, false);
      }
      if (key !== null) {
        var count = (holds.get(key) || 0) + 1;
        holds.set(key, count);
        if (count === 1) setPressed(key, true);
      }
      fingers.set(id, key);
    }

    function lift(id) {
      move(id, null);
      fingers.delete(id);
    }

    bar.addEventListener('pointerdown', function (e) {
      e.preventDefault();
      bar.setPointerCapture(e.pointerId); // keep getting this finger's events off the bar
      move(e.pointerId, keyAt(e.clientX, e.clientY));
    });
    bar.addEventListener('pointermove', function (e) {
      if (fingers.has(e.pointerId)) move(e.pointerId, keyAt(e.clientX, e.clientY));
    });
    bar.addEventListener('pointerup', function (e) { lift(e.pointerId); });
    bar.addEventListener('pointercancel', function (e) { lift(e.pointerId); });
    bar.addEventListener('contextmenu', function (e) { e.preventDefault(); });

    // Never leave a key stuck down when the page loses focus.
    function releaseAll() { Array.from(fingers.keys()).forEach(lift); }
    window.addEventListener('blur', releaseAll);
    document.addEventListener('visibilitychange', function () { if (document.hidden) releaseAll(); });

    return bar;
  }

  // The usual mobile web game setup: the page never zooms, scrolls, selects
  // text or opens long-press menus, so every touch goes to the game.
  // CSS covers most of it (.tc-mobile in touch-controls.css). iOS Safari
  // ignores user-scalable=no for pinch, so its gesture events are blocked too.
  function lockPage() {
    document.documentElement.classList.add('tc-mobile');
    var block = function (e) { e.preventDefault(); };
    ['gesturestart', 'gesturechange', 'gestureend', 'dblclick', 'contextmenu'].forEach(function (type) {
      document.addEventListener(type, block, { passive: false });
    });
    // A second finger landing anywhere must not start a pinch.
    document.addEventListener('touchmove', function (e) {
      if (e.touches.length > 1) e.preventDefault();
    }, { passive: false });
  }

  // The least height the bar needs: its tallest group plus its padding.
  // The game gets the rest of the screen. The bar may grow beyond this.
  function minHeight(bar) {
    var style = getComputedStyle(bar);
    var content = Math.max(bar.querySelector('.tc-dpad').offsetHeight, bar.querySelector('.tc-actions').offsetHeight);
    return content + parseFloat(style.paddingTop) + parseFloat(style.paddingBottom);
  }

  // Sizes the canvas with CSS. The canvas's own width and height are the
  // game's resolution, set by the game itself.
  function fit(canvas, bar) {
    var w = canvas.width, h = canvas.height;
    if (!w || !h) return;
    var cssW;
    if (bar) {
      // Touch layout: full width, with the bar's height kept free below.
      // The body's padding holds the notch and home-indicator insets.
      var body = getComputedStyle(document.body);
      var roomW = document.body.clientWidth - parseFloat(body.paddingLeft) - parseFloat(body.paddingRight);
      var roomH = document.body.clientHeight - parseFloat(body.paddingTop) - parseFloat(body.paddingBottom) - minHeight(bar);
      cssW = Math.min(roomW, roomH * w / h);
    } else {
      // Keyboard layout: a whole number of physical pixels per game pixel,
      // when that reaches at least 2. Below 2x, rounding down would waste up
      // to half the window, so the game fills the window at a fractional
      // scale instead. This also shrinks a game larger than the window.
      var dpr = window.devicePixelRatio || 1;
      var fill = Math.min(innerWidth / w, innerHeight / h);
      var scale = Math.floor(fill * dpr);
      cssW = scale >= 2 ? w * scale / dpr : w * fill;
    }
    canvas.style.width = cssW + 'px';
    canvas.style.height = (cssW * h / w) + 'px';
  }

  function setup(options) {
    var bar = null;
    if (shouldShow()) {
      lockPage();
      bar = attach(options.frame, options);
    }
    var refit = function () { fit(options.canvas, bar); };
    window.addEventListener('resize', refit);
    window.addEventListener('orientationchange', function () { setTimeout(refit, 100); });
    if (window.visualViewport) window.visualViewport.addEventListener('resize', refit);
    return { bar: bar, fit: refit };
  }

  window.TouchControls = {
    setup: setup,
    attach: attach,
    lockPage: lockPage,
    shouldShow: shouldShow,
    minHeight: minHeight,
    fit: fit,
  };
})();
