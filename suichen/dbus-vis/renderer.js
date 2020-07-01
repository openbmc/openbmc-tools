// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.

class Renderer {
  constructor() {
    let c1 = document.getElementById('my_canvas_ipmi');
    let c2 = document.getElementById('my_canvas_dbus');
    let c3 = document.getElementById('my_canvas_boost_asio_handler');
    this.canvas1 = c1;
    this.canvas2 = c2;
    this.canvas3 = c3;
    this.width1 = c1.width;
    this.height1 = c1.height;
    this.width2 = c2.width;
    this.height2 = c2.height;
    this.width3 = c3.width;
    this.height3 = c3.height;
    this.ctx1 = this.canvas1.getContext('2d');
    this.ctx2 = this.canvas2.getContext('2d');
    this.ctx3 = this.canvas3.getContext('2d');
    this.frame_count = 0;
    this.addBindings();
    this.addListeners();
    this.update();
    this.run();
  }

  addBindings() {
    this.update = this.update.bind(this);
    this.run = this.run.bind(this);
  }

  addListeners() {
    window.addEventListener('resize', this.update);
  }

  update() {
    console.log('update, ' + window.innerWidth + ' x ' + window.innerHeight);
    if (false) {
      this.width1 = window.innerWidth;
      this.height1 = window.innerHeight;
      this.canvas1.width = this.width1;
      this.canvas1.height = this.height1;
    }
  }

  run() {
    draw_timeline(this.ctx1);
    draw_timeline_dbus(this.ctx2);
    draw_timeline_boost_asio_handler(this.ctx3);
    window.requestAnimationFrame(this.run);
  }
}

g_renderer = new Renderer();
