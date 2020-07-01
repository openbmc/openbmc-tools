const {dialog} = require('electron').remote;
const {fs} = require('file-system');
const {util} = require('util');
const {exec} = require('child_process');

// Main view object
var ipmi_timeline_view = new IPMITimelineView();

var btn_start_capture = document.getElementById('btn_start_capture');
var select_capture_mode = document.getElementById('select_capture_mode');
var capture_info = document.getElementById('capture_info');

var radio_open_file = document.getElementById('radio_open_file');
var radio_capture = document.getElementById('radio_capture');
var title_open_file = document.getElementById('title_open_file');
var title_capture = document.getElementById('title_capture');

// Set up Electron-related stuff here; Electron does not allow inlining button
// events
document.getElementById('c1').addEventListener(
    'click', OnGroupByConditionChanged);  // NetFN
document.getElementById('c2').addEventListener(
    'click', OnGroupByConditionChanged);  // CMD

// Zoom in button
document.getElementById('btn_zoom_in').addEventListener('click', function() {
  BeginZoomAnimation(0.5);
  ipmi_timeline_view.BeginZoomAnimation(0.5);
  boost_asio_handler_timeline_view.BeginZoomAnimation(0.5);
});

// Zoom out button
document.getElementById('btn_zoom_out').addEventListener('click', function() {
  BeginZoomAnimation(-1);
  ipmi_timeline_view.BeginZoomAnimation(-1);
  boost_asio_handler_timeline_view.BeginZoomAnimation(-1);
});

// Pan left button
document.getElementById('btn_pan_left').addEventListener('click', function() {
  BeginPanScreenAnimaton(-0.5);
  ipmi_timeline_view.BeginPanScreenAnimaton(-0.5);
  boost_asio_handler_timeline_view.BeginPanScreenAnimaton(-0.5);
});

// Pan right button
document.getElementById('btn_pan_right').addEventListener('click', function() {
  BeginPanScreenAnimaton(0.5);
  ipmi_timeline_view.BeginPanScreenAnimaton(0.5);
  boost_asio_handler_timeline_view.BeginPanScreenAnimaton(0.5);
});

// Reset zoom button
document.getElementById('btn_zoom_reset').addEventListener('click', function() {
  BeginSetBoundaryAnimation(RANGE_LEFT_INIT, RANGE_RIGHT_INIT)
  ipmi_timeline_view.BeginSetBoundaryAnimation(
      RANGE_LEFT_INIT, RANGE_RIGHT_INIT)
  dbus_timeline_view.BeginSetBoundaryAnimation(
      RANGE_LEFT_INIT, RANGE_RIGHT_INIT)
  boost_asio_handler_timeline_view.BeginSetBoundaryAnimation(
      RANGE_LEFT_INIT, RANGE_RIGHT_INIT)
})

// Generate replay
document.getElementById('gen_replay_ipmitool1')
    .addEventListener('click', function() {
      GenerateIPMIToolIndividualCommandReplay(HighlightedRequests);
    });
document.getElementById('gen_replay_ipmitool2')
    .addEventListener('click', function() {
      GenerateIPMIToolExecListReplay(HighlightedRequests);
    });
document.getElementById('gen_replay_ipmid_legacy')
    .addEventListener('click', function() {
      GenerateBusctlReplayLegacyInterface(HighlightedRequests);
    });
document.getElementById('gen_replay_ipmid_new')
    .addEventListener('click', function() {
      GenerateBusctlReplayNewInterface(HighlightedRequests);
    });
document.getElementById('btn_start_capture')
    .addEventListener('click', function() {
      let h = document.getElementById('text_hostname').value;
      StartCapture(h);
    });

// For capture mode
document.getElementById('btn_stop_capture')
    .addEventListener('click', function() {
      StopCapture();
    });
document.getElementById('select_capture_mode')
    .addEventListener('click', OnCaptureModeChanged);
radio_open_file.addEventListener('click', OnAppModeChanged);
radio_capture.addEventListener('click', OnAppModeChanged);

radio_open_file.click();

// App mode: open file or capture
function OnAppModeChanged() {
  title_open_file.style.display = 'none';
  title_capture.style.display = 'none';
  if (radio_open_file.checked) {
    title_open_file.style.display = 'block';
  }
  if (radio_capture.checked) {
    title_capture.style.display = 'block';
  }
}

// Capture mode: Live capture or staged capture
function OnCaptureModeChanged() {
  let x = select_capture_mode;
  let i = capture_info;
  let desc = '';
  switch (x.value) {
    case 'live':
      desc = 'Live: read BMC\'s dbus-monitor console output directly';
      g_capture_mode = 'live';
      break;
    case 'staged':
      desc =
          'Staged: Store BMC\'s dbus-monitor output in a file and transfer back for display';
      g_capture_mode = 'staged';
      break;
  }
  i.textContent = desc;
}

function OnCaptureStart() {
  switch (g_capture_state) {
    case 'not started':
      capture_info.textContent = 'dbus-monitor running on BMC';
      break;
    default:
      break;
  }
}

function OnCaptureStop() {
  btn_start_capture.disabled = false;
  select_capture_mode.disabled = false;
  text_hostname.disabled = false;
}

// Data
var HistoryHistogram = [];
// var Data_IPMI = []

// =====================

let Intervals = [];
let Titles = [];
let HighlightedRequests = [];
let GroupBy = [];
let GroupByStr = '';

// (NetFn, Cmd) -> [ Bucket Indexes ]
// Normalized (0~1) bucket index for the currently highlighted IPMI requests
let IpmiVizHistHighlighted = {};
let HistogramThresholds = {};

function IsIntersected(i0, i1) {
  return (!((i0[1] < i1[0]) || (i0[0] > i1[1])));
}

var NetFnCmdToDescription = {
  '6, 1': 'App-GetDeviceId',
  '6, 3': 'App-WarmReset',
  '10, 64': 'Storage-GetSelInfo',
  '10, 35': 'Storage-GetSdr',
  '4, 32': 'Sensor-GetDeviceSDRInfo',
  '4, 34': 'Sensor-ReserveDeviceSDRRepo',
  '4, 47': 'Sensor-GetSensorType',
  '10, 34': 'Storage-ReserveSdrRepository',
  '46, 50': 'OEM Extension',
  '4, 39': 'Sensor-GetSensorThresholds',
  '4, 45': 'Sensor-GetSensorReading',
  '10, 67': 'Storage-GetSelEntry',
  '58, 196': 'IBM_OEM',
  '10, 32': 'Storage-GetSdrRepositoryInfo',
  '4, 33': 'Sensor-GetDeviceSDR',
  '6, 54': 'App-Get BT Interface Capabilities',
  '10, 17': 'Storage-ReadFruData',
  '10, 16': 'Storage-GetFruInventoryAreaInfo',
  '4, 2': 'Sensor-PlatformEvent',
  '4, 48': 'Sensor-SetSensor',
  '6, 34': 'App-ResetWatchdogTimer'
};

const CANVAS_H = document.getElementById('my_canvas_ipmi').height;
const CANVAS_W = document.getElementById('my_canvas_ipmi').width;

var LowerBoundTime = RANGE_LEFT_INIT;
var UpperBoundTime = RANGE_RIGHT_INIT;
var LastTimeLowerBound;
var LastTimeUpperBound;
// Dirty flags for determining when to redraw the canvas
let IsCanvasDirty = true;
let IsHighlightDirty = false;
// Animating left and right boundaries
let IsAnimating = false;
let LowerBoundTimeTarget = LowerBoundTime;
let UpperBoundTimeTarget = UpperBoundTime;
// For keyboard interaction: arrow keys and Shift
let CurrDeltaX = 0;         // Proportion of Canvas to scroll per frame.
let CurrDeltaZoom = 0;      // Delta zoom per frame.
let CurrShiftFlag = false;  // Whether the Shift key is depressed

const LEFT_MARGIN = 640
const RIGHT_MARGIN = 1390;
const LINE_HEIGHT = 15;
const LINE_SPACING = 17;
const YBEGIN = 22 + LINE_SPACING;
const TEXT_Y0 = 3;
const HISTOGRAM_W = 100, HISTOGRAM_H = LINE_SPACING;
const HISTOGRAM_X = 270;
// If some request's time is beyond the right tail, it's considered "too long"
// If some request's time is below the left tail it's considered "good"
const HISTOGRAM_LEFT_TAIL_WIDTH = 0.05, HISTOGRAM_RIGHT_TAIL_WIDTH = 0.05;

let IpmiVizHistogramImageData = {};  // Image data for rendered histogram

// Input is the data that's completed layout
// is_free_x:     Should each histogram has its own X range or not
// num_buckets: # of buckets for histograms
// theta: top and bottom portion to cut
function ComputeHistogram(num_buckets = 30, is_free_x = true) {
  let global_lb = Infinity, global_ub = -Infinity;
  IpmiVizHistogramImageData = {};
  // Global minimal and maximal values
  for (let i = 0; i < Intervals.length; i++) {
    let interval = Intervals[i];
    let l = Math.min.apply(Math, interval.map(function(x) {
      return x[1] - x[0];
    }));
    let u = Math.max.apply(Math, interval.map(function(x) {
      return x[1] - x[0];
    }));
    global_lb = Math.min(l, global_lb);
    global_ub = Math.max(u, global_ub);
  }

  console.log('global lb ub:' + global_lb + ' ' + global_ub);

  HistoryHistogram = [];
  for (let i = 0; i < Intervals.length; i++) {
    let interval = Intervals[i];
    let lb = global_lb, ub = global_ub;
    if (is_free_x == true) {
      lb = Math.min.apply(Math, interval.map(function(x) {
        return x[1] - x[0];
      }));
      ub = Math.max.apply(Math, interval.map(function(x) {
        return x[1] - x[0];
      }));
    }
    const EPS = 1e-2;
    if (lb == ub) ub = lb + EPS;
    let line = [lb * 1000000, ub * 1000000];  // to usec
    let buckets = [];
    for (let j = 0; j < num_buckets; j++) buckets.push(0);
    for (let j = 0; j < interval.length; j++) {
      let t = interval[j][1] - interval[j][0];
      let bucket_idx = parseInt(t / ((ub - lb) / num_buckets));
      buckets[bucket_idx]++;
    }
    line.push(buckets);
    HistoryHistogram[Titles[i]] = line;
  }
}

function Preprocess(data) {
  preprocessed = [];
  let StartingUsec_IPMI;

  if (g_StartingSec == undefined) {
    StartingUsec_IPMI = undefined;
  } else {
    StartingUsec_IPMI = g_StartingSec * 1000000;
  }

  for (let i = 0; i < data.length; i++) {
    let entry = data[i].slice();
    let lb = entry[2], ub = entry[3];

    // Only when IPMI view is present (i.e. no DBus pcap is loaded)
    if (i == 0 && StartingUsec_IPMI == undefined) {
      StartingUsec_IPMI = lb;
    }

    entry[2] = lb - StartingUsec_IPMI;
    entry[3] = ub - StartingUsec_IPMI;
    preprocessed.push(entry);
  }
  return preprocessed;
}

function Group(data, groupBy) {
  let grouped = {};
  const idxes = {'NetFN': 0, 'CMD': 1};
  for (let n = 0; n < data.length; n++) {
    let key = '';
    for (let i = 0; i < groupBy.length; i++) {
      if (i > 0) {
        key += ', ';
      }
      key += data[n][idxes[groupBy[i]]];
    }
    if (grouped[key] == undefined) {
      grouped[key] = [];
    }
    grouped[key].push(data[n]);
  }
  return grouped;
}

function GenerateTimeLine(grouped) {
  const keys = Object.keys(grouped);
  let sortedKeys = keys.slice();
  // If NetFN and CMD are both selected, sort by NetFN then CMD
  // In this case, all "keys" are string-encoded integer pairs
  if (keys.length > 0 && ipmi_timeline_view.GroupBy.length == 2) {
    sortedKeys = sortedKeys.sort(function(a, b) {
      a = a.split(',');
      b = b.split(',');
      let aa = parseInt(a[0]) * 256 + parseInt(a[1]);
      let bb = parseInt(b[0]) * 256 + parseInt(b[1]);
      return aa < bb ? -1 : (aa > bb ? 1 : 0);
    });
  }

  Intervals = [];
  Titles = [];
  for (let i = 0; i < sortedKeys.length; i++) {
    Titles.push(sortedKeys[i]);
    line = [];
    for (let j = 0; j < grouped[sortedKeys[i]].length; j++) {
      let entry = grouped[sortedKeys[i]][j];
      // Lower bound, Upper bound, and a reference to the original request
      line.push([
        parseFloat(entry[2]) / 1000000, parseFloat(entry[3]) / 1000000, entry
      ]);
    }
    Intervals.push(line);
  }

  ipmi_timeline_view.Intervals = Intervals.slice();
  ipmi_timeline_view.Titles = Titles.slice();
}

function OnGroupByConditionChanged() {
  const tags = ['c1', 'c2'];
  const v = ipmi_timeline_view;
  v.GroupBy = [];
  v.GroupByStr = '';
  for (let i = 0; i < tags.length; i++) {
    let cb = document.getElementById(tags[i]);
    if (cb.checked) {
      v.GroupBy.push(cb.value);
      if (v.GroupByStr.length > 0) {
        v.GroupByStr += ', ';
      }
      v.GroupByStr += cb.value;
    }
  }
  let preproc = Preprocess(Data_IPMI);
  grouped = Group(preproc, v.GroupBy);
  GenerateTimeLine(grouped);

  IsCanvasDirty = true;
  ipmi_timeline_view.IsCanvasDirty = true;
}

function MapXCoord(x, left_margin, right_margin, rl, rr) {
  let ret = left_margin + (x - rl) / (rr - rl) * (right_margin - left_margin);
  if (ret < left_margin) {
    ret = left_margin;
  } else if (ret > right_margin) {
    ret = right_margin;
  }
  return ret;
}

function Zoom(dz, mid = undefined) {
  if (CurrShiftFlag) dz *= 2;
  if (dz != 0) {
    if (mid == undefined) {
      mid = (LowerBoundTime + UpperBoundTime) / 2;
    }
    LowerBoundTime = mid - (mid - LowerBoundTime) * (1 - dz);
    UpperBoundTime = mid + (UpperBoundTime - mid) * (1 - dz);
    IsCanvasDirty = true;
    IsAnimating = false;
  }
}

function BeginZoomAnimation(dz, mid = undefined) {
  if (mid == undefined) {
    mid = (LowerBoundTime + UpperBoundTime) / 2;
  }
  LowerBoundTimeTarget = mid - (mid - LowerBoundTime) * (1 - dz);
  UpperBoundTimeTarget = mid + (UpperBoundTime - mid) * (1 - dz);
  IsCanvasDirty = true;
  IsAnimating = true;
}

function BeginPanScreenAnimaton(delta_screens) {
  let deltat = (UpperBoundTime - LowerBoundTime) * delta_screens;
  BeginSetBoundaryAnimation(LowerBoundTime + deltat, UpperBoundTime + deltat);
}

function BeginSetBoundaryAnimation(lt, rt) {
  IsAnimating = true;
  LowerBoundTimeTarget = lt;
  UpperBoundTimeTarget = rt;
}

function BeginWarpToRequestAnimation(req) {
  let mid_new = (req[0] + req[1]) / 2;
  let mid = (LowerBoundTime + UpperBoundTime) / 2;
  let lt = LowerBoundTime + (mid_new - mid);
  let rt = UpperBoundTime + (mid_new - mid);
  BeginSetBoundaryAnimation(lt, rt);
}

function UpdateAnimation() {
  const EPS = 1e-3;
  if (Math.abs(LowerBoundTime - LowerBoundTimeTarget) < EPS &&
      Math.abs(UpperBoundTime - UpperBoundTimeTarget) < EPS) {
    LowerBoundTime = LowerBoundTimeTarget;
    UpperBoundTime = UpperBoundTimeTarget;
    IsAnimating = false;
  }
  if (IsAnimating) {
    let t = 0.80;
    LowerBoundTime = LowerBoundTime * t + LowerBoundTimeTarget * (1 - t);
    UpperBoundTime = UpperBoundTime * t + UpperBoundTimeTarget * (1 - t);
    IsCanvasDirty = true;
  }
}

function GetHistoryHistogram() {
  return HistoryHistogram;
}

function RenderHistogramForImageData(ctx, key) {
  let PAD = 1,   // To make up for the extra stroke width
      PAD2 = 2;  // To preserve some space at both ends of the histogram

  let cumDensity0 = 0, cumDensity1 = 0;

  //      Left normalized index  Left value  Right normalized index, Right value
  let threshEntry = [[undefined, undefined], [undefined, undefined]];
  const x = 0, y = 0, w = HISTOGRAM_W, h = HISTOGRAM_H;
  let hist = GetHistoryHistogram()[key];
  if (hist == undefined) return;

  let buckets = hist[2];
  let dw = w * 1.0 / buckets.length;
  let maxCount = 0, totalCount = 0;
  for (let i = 0; i < buckets.length; i++) {
    if (maxCount < buckets[i]) {
      maxCount = buckets[i];
    }
    totalCount += buckets[i];
  }
  ctx.fillStyle = '#FFF';
  ctx.fillRect(x, y, w, h);

  ctx.strokeStyle = '#AAA';
  ctx.fillStyle = '#000';
  ctx.lineWidth = 1;
  ctx.strokeRect(x + PAD, y + PAD, w - 2 * PAD, h - 2 * PAD);
  for (let i = 0; i < buckets.length; i++) {
    const bucketsLen = buckets.length;
    if (buckets[i] > 0) {
      let dx0 = x + PAD2 + (w - 2 * PAD2) * 1.0 * i / buckets.length,
          dx1 = x + PAD2 + (w - 2 * PAD2) * 1.0 * (i + 1) / buckets.length,
          dy0 = y + h - h * 1.0 * buckets[i] / maxCount, dy1 = y + h;
      let delta_density = buckets[i] / totalCount;
      cumDensity0 = cumDensity1;
      cumDensity1 += delta_density;

      // Write thresholds
      if (cumDensity0 < HISTOGRAM_LEFT_TAIL_WIDTH &&
          cumDensity1 >= HISTOGRAM_LEFT_TAIL_WIDTH) {
        threshEntry[0][0] = i / buckets.length;
        threshEntry[0][1] = hist[0] + (hist[1] - hist[0]) / bucketsLen * i;
      }
      if (cumDensity0 < 1 - HISTOGRAM_RIGHT_TAIL_WIDTH &&
          cumDensity1 >= 1 - HISTOGRAM_RIGHT_TAIL_WIDTH) {
        threshEntry[1][0] = (i - 1) / buckets.length;
        threshEntry[1][1] =
            hist[0] + (hist[1] - hist[0]) / bucketsLen * (i - 1);
      }

      ctx.fillRect(dx0, dy0, dx1 - dx0, dy1 - dy0);
    }
  }

  // Mark the threshold regions
  ctx.fillStyle = 'rgba(0,255,0,0.1)';
  let dx = x + PAD2;
  dw = (w - 2 * PAD2) * 1.0 * threshEntry[0][0];
  ctx.fillRect(dx, y, dw, h);

  ctx.fillStyle = 'rgba(255,0,0,0.1)';
  ctx.beginPath();
  dx = x + PAD2 + (w - 2 * PAD2) * 1.0 * threshEntry[1][0];
  dw = (w - 2 * PAD2) * 1.0 * (1 - threshEntry[1][0]);
  ctx.fillRect(dx, y, dw, h);

  IsCanvasDirty = true;
  return [ctx.getImageData(x, y, w, h), threshEntry];
}

function RenderHistogram(ctx, key, xMid, yMid) {
  if (GetHistoryHistogram()[key] == undefined) {
    return;
  }
  if (IpmiVizHistogramImageData[key] == undefined) {
    return;
  }
  let hist = GetHistoryHistogram()[key];
  ctx.putImageData(
      IpmiVizHistogramImageData[key], xMid - HISTOGRAM_W / 2,
      yMid - HISTOGRAM_H / 2);

  let ub = '';  // Upper bound label
  ctx.textAlign = 'left';
  ctx.fillStyle = '#000';
  if (hist[1] > 1000) {
    ub = (hist[1] / 1000.0).toFixed(1) + 'ms';
  } else {
    ub = hist[1].toFixed(1) + 'us';
  }
  ctx.fillText(ub, xMid + HISTOGRAM_W / 2, yMid);

  let lb = '';  // Lower bound label
  if (hist[0] > 1000) {
    lb = (hist[0] / 1000.0).toFixed(1) + 'ms';
  } else {
    lb = hist[0].toFixed(1) + 'us';
  }
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  ctx.fillText(lb, xMid - HISTOGRAM_W / 2, yMid);
}

function draw_timeline(ctx) {
  ipmi_timeline_view.Render(ctx);
  return;

  // Update
  let toFixedPrecision = 2;
  if (UpperBoundTime - LowerBoundTime < 0.1) {
    toFixedPrecision = 4;
  } else if (UpperBoundTime - LowerBoundTime < 1) {
    toFixedPrecision = 3;
  }
  let dx = CurrDeltaX;
  if (dx != 0) {
    let extent = UpperBoundTime - LowerBoundTime;
    if (CurrShiftFlag) dx *= 5;
    LowerBoundTime += dx * extent;
    UpperBoundTime += dx * extent;
    IsCanvasDirty = true;
  }

  // Hovered IPMI interval (for display) and its corresponding original request
  let theHoveredReq = undefined;
  let theHoveredInterval = undefined;
  let currHighlightedReqs = [];

  let dz = CurrDeltaZoom;
  Zoom(dz);
  UpdateAnimation();

  LastTimeLowerBound = LowerBoundTime;
  LastTimeUpperBound = UpperBoundTime;

  if (IsCanvasDirty) {
    IsCanvasDirty = false;
    // Shorthand for HighlightedRegion.t{0,1}
    let t0 = undefined;
    let t1 = undefined;

    // Highlight
    let highlightedInterval = [];
    numIntersected = 0;  // How many requests intersect with highlighted area
    if (IsHighlighted()) {
      t0 = Math.min(HighlightedRegion.t0, HighlightedRegion.t1);
      t1 = Math.max(HighlightedRegion.t0, HighlightedRegion.t1);
      highlightedInterval = [t0, t1];
    }
    IpmiVizHistHighlighted = {};

    ctx.globalCompositeOperation = 'source-over';
    ctx.clearRect(0, 0, CANVAS_W, CANVAS_H);
    ctx.strokeStyle = '#000';
    ctx.fillStyle = '#000';
    ctx.lineWidth = 1;

    const width = CANVAS_W;
    const height = CANVAS_H;

    ctx.font = '12px Monospace';

    // Highlight current line
    if (MouseState.hoveredLineIndex >= 0 &&
        MouseState.hoveredLineIndex < Intervals.length) {
      ctx.fillStyle = 'rgba(192,192,255,0.8)';
      let dy = YBEGIN + LINE_SPACING * MouseState.hoveredLineIndex -
          LINE_SPACING / 2;
      ctx.fillRect(0, dy, width, LINE_SPACING);
    }

    ctx.fillStyle = '#000';
    // Time marks at the beginning & end of the visible range
    ctx.textBaseline = 'bottom';
    ctx.textAlign = 'left';
    ctx.fillText(
        '' + LowerBoundTime.toFixed(toFixedPrecision) + 's', LEFT_MARGIN + 3,
        height);
    ctx.textAlign = 'end';
    ctx.fillText(
        '' + UpperBoundTime.toFixed(toFixedPrecision) + 's', RIGHT_MARGIN - 3,
        height);

    ctx.textBaseline = 'top';
    ctx.textAlign = 'left';
    ctx.fillText(
        '' + LowerBoundTime.toFixed(toFixedPrecision) + 's', LEFT_MARGIN + 3,
        TEXT_Y0);
    ctx.textAlign = 'right';
    ctx.fillText(
        '' + UpperBoundTime.toFixed(toFixedPrecision) + 's', RIGHT_MARGIN - 3,
        TEXT_Y0);

    let y = YBEGIN;
    let numVisibleRequests = 0;

    ctx.beginPath();
    ctx.moveTo(LEFT_MARGIN, 0);
    ctx.lineTo(LEFT_MARGIN, height);
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(RIGHT_MARGIN, 0);
    ctx.lineTo(RIGHT_MARGIN, height);
    ctx.stroke();

    // Column Titles
    ctx.fillStyle = '#000';
    let columnTitle = '(All requests)';
    if (GroupByStr.length > 0) {
      columnTitle = GroupByStr;
    }
    ctx.textAlign = 'right';
    ctx.textBaseline = 'top';
    ctx.fillText(columnTitle, LEFT_MARGIN - 3, 3);

    // "Histogram" title
    ctx.fillStyle = '#000';
    ctx.textBaseline = 'top';
    ctx.textAlign = 'center';
    ctx.fillText('Distribution', HISTOGRAM_X, TEXT_Y0);

    ctx.textAlign = 'right'
    ctx.fillText('In dataset /', HISTOGRAM_X, TEXT_Y0 + LINE_SPACING - 2);

    ctx.fillStyle = '#00F';

    ctx.textAlign = 'left'
    if (IsHighlighted()) {
      ctx.fillText(' In selection', HISTOGRAM_X, TEXT_Y0 + LINE_SPACING - 2);
    }
    else {
      ctx.fillText(' In viewport', HISTOGRAM_X, TEXT_Y0 + LINE_SPACING - 2);
    }

    ctx.fillStyle = '#000';

    // Time Axis Breaks
    const breakWidths = [
      3600,   60,     30,     10,      5,       2,      1,     0.5,
      0.2,    0.1,    0.05,   0.02,    0.01,    0.005,  0.002, 0.001,
      0.0005, 0.0002, 0.0001, 0.00005, 0.00002, 0.00001
    ];
    let bidx = 0;
    while (bidx < breakWidths.length &&
           breakWidths[bidx] > UpperBoundTime - LowerBoundTime) {
      bidx++;
    }
    let breakWidth = breakWidths[bidx + 1];
    if (bidx < breakWidths.length) {
      let t2 = 0;  // Cannot name as "t0" otherwise clash
      bidx = 0;
      while (bidx < breakWidths.length) {
        while (t2 + breakWidths[bidx] < LowerBoundTime) {
          t2 += breakWidths[bidx];
        }
        if (t2 + breakWidths[bidx] >= LowerBoundTime &&
            t2 + breakWidths[bidx] <= UpperBoundTime) {
          break;
        }
        bidx++;
      }
      if (bidx < breakWidths.length) {
        for (; t2 < UpperBoundTime; t2 += breakWidth) {
          if (t2 > LowerBoundTime) {
            ctx.beginPath();
            let dx = MapXCoord(
                t2, LEFT_MARGIN, RIGHT_MARGIN, LowerBoundTime, UpperBoundTime);
            ctx.strokeStyle = '#C0C0C0';
            ctx.moveTo(dx, 0);
            ctx.lineTo(dx, height);
            ctx.stroke();
            ctx.closePath();
            ctx.fillStyle = '#C0C0C0';

            ctx.textAlign = 'left';
            ctx.textBaseline = 'bottom';
            let label2 = t2.toFixed(toFixedPrecision) + 's';
            let w = ctx.measureText(label2).width;
            if (dx + w > RIGHT_MARGIN) ctx.textAlign = 'right';
            ctx.fillText(label2, dx, height);

            ctx.textBaseline = 'top';
            ctx.fillText(label2, dx, TEXT_Y0);
          }
        }
      }
    }

    // Whether we aggregate selected requests or visible requests
    let isAggregateSelection = false;
    if (IsHighlighted()) isAggregateSelection = true;
    let numVisibleRequestsPerLine = [];
    let totalSecondsPerLine = [];

    for (let j = 0; j < Intervals.length; j++) {
      ctx.textBaseline = 'middle';
      ctx.textAlign = 'right';
      let desc_width = 0;
      if (NetFnCmdToDescription[Titles[j]] != undefined) {
        let desc = ' (' + NetFnCmdToDescription[Titles[j]] + ')';
        desc_width = ctx.measureText(desc).width;
        ctx.fillStyle = '#888';  // Grey
        ctx.fillText(desc, LEFT_MARGIN - 3, y);
      }

      // Plot histogram
      if (true) {
        if (GetHistoryHistogram()[Titles[j]] != undefined) {
          if (IpmiVizHistogramImageData[Titles[j]] == undefined) {
            let tmp = RenderHistogramForImageData(ctx, Titles[j]);
            IpmiVizHistogramImageData[Titles[j]] = tmp[0];
            HistogramThresholds[Titles[j]] = tmp[1];
          }
          RenderHistogram(ctx, Titles[j], HISTOGRAM_X, y);
          ctx.textAlignment = 'right';
        } else {
        }
      }
      ctx.textAlignment = 'right';
      ctx.textBaseline = 'middle';
      ctx.fillStyle = '#000000';  // Revert to Black
      ctx.strokeStyle = '#000000';
      ctx.fillText(Titles[j], LEFT_MARGIN - 3 - desc_width, y);

      let numOverflowEntriesToTheLeft = 0;   // #entries below the lower bound
      let numOverflowEntriesToTheRight = 0;  // #entries beyond the upper bound
      let numVisibleRequestsCurrLine = 0;    // #entries visible
      let totalSecsCurrLine = 0;             // Total duration in seconds

      for (let i = 0; i < Intervals[j].length; i++) {
        let lb = Intervals[j][i][0], ub = Intervals[j][i][1];
        if (lb > ub) continue;  // Unmatched (only enter & no exit timestamp)

        let isHighlighted = false;
        let durationUsec = (Intervals[j][i][1] - Intervals[j][i][0]) * 1000000;
        let lbub = [lb, ub];
        if (IsHighlighted()) {
          if (IsIntersected(lbub, highlightedInterval)) {
            numIntersected++;
            isHighlighted = true;
            currHighlightedReqs.push(Intervals[j][i][2]);
          }
        }

        if (ub < LowerBoundTime) {
          numOverflowEntriesToTheLeft++;
          continue;
        }
        if (lb > UpperBoundTime) {
          numOverflowEntriesToTheRight++;
          continue;
        }

        let dx0 = MapXCoord(
                lb, LEFT_MARGIN, RIGHT_MARGIN, LowerBoundTime, UpperBoundTime),
            dx1 = MapXCoord(
                ub, LEFT_MARGIN, RIGHT_MARGIN, LowerBoundTime, UpperBoundTime),
            dy0 = y - LINE_HEIGHT / 2, dy1 = y + LINE_HEIGHT / 2;

        dx0 = Math.max(dx0, LEFT_MARGIN);
        dx1 = Math.min(dx1, RIGHT_MARGIN);
        let dw = Math.max(0, dx1 - dx0);

        if (isHighlighted) {
          ctx.fillStyle = 'rgba(128,128,255,0.5)';
          ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
        }

        // Intersect with mouse using pixel coordinates
        if (IsIntersected([dx0, dx0 + dw], [MouseState.x, MouseState.x]) &&
            IsIntersected([dy0, dy1], [MouseState.y, MouseState.y])) {
          ctx.fillStyle = 'rgba(255,255,0,0.5)';
          ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
          theHoveredReq = Intervals[j][i][2];
          theHoveredInterval = Intervals[j][i];
        }

        ctx.lineWidth = 0.5;

        // If this request is taking too long/is quick enough, use red/green
        let entry = HistogramThresholds[Titles[j]];
        if (entry != undefined) {
          if (entry[0][1] != undefined && durationUsec < entry[0][1]) {
            ctx.strokeStyle = '#0F0';
          } else if (entry[1][1] != undefined && durationUsec > entry[1][1]) {
            ctx.strokeStyle = '#A00';
          } else {
            ctx.strokeStyle = '#000';
          }
        } else {
          ctx.strokeStyle = '#000';
        }

        ctx.strokeRect(dx0, dy0, dw, dy1 - dy0);
        numVisibleRequests++;

        // Affects whether this req will be reflected in the aggregate info
        //     section
        if ((isAggregateSelection == false) ||
            (isAggregateSelection == true && isHighlighted == true)) {
          numVisibleRequestsCurrLine++;
          totalSecsCurrLine += (Intervals[j][i][1] - Intervals[j][i][0]);

          // If a histogram exists for Titles[j], process the highlighted
          //     histogram buckets
          if (GetHistoryHistogram()[Titles[j]] != undefined) {
            let histogramEntry = GetHistoryHistogram()[Titles[j]];
            let bucketInterval = (histogramEntry[1] - histogramEntry[0]) /
                histogramEntry[2].length;
            let bucketIndex =
                Math.floor(
                    (durationUsec - histogramEntry[0]) / bucketInterval) /
                histogramEntry[2].length;

            if (IpmiVizHistHighlighted[Titles[j]] == undefined) {
              IpmiVizHistHighlighted[Titles[j]] = new Set();
            }
            let entry = IpmiVizHistHighlighted[Titles[j]];
            entry.add(bucketIndex);
          }
        }
      }

      // Triangle markers for entries outside of the viewport
      {
        const PAD = 2, H = LINE_SPACING;
        if (MouseState.hoveredLineIndex == j &&
            MouseState.hoveredSide == 'left') {
          ctx.fillStyle = '#0000FF';
        } else {
          ctx.fillStyle = 'rgba(128,128,0,0.5)';
        }
        if (numOverflowEntriesToTheLeft > 0) {
          ctx.beginPath();
          ctx.moveTo(LEFT_MARGIN + PAD + H / 2, y - H / 2);
          ctx.lineTo(LEFT_MARGIN + PAD, y);
          ctx.lineTo(LEFT_MARGIN + PAD + H / 2, y + H / 2);
          ctx.closePath();
          ctx.fill();
          ctx.textAlign = 'left';
          ctx.textBaseline = 'center';
          ctx.fillText(
              '+' + numOverflowEntriesToTheLeft, LEFT_MARGIN + 2 * PAD + H / 2,
              y);
        }

        if (MouseState.hoveredLineIndex == j &&
            MouseState.hoveredSide == 'right') {
          ctx.fillStyle = '#0000FF';
        } else {
          ctx.fillStyle = 'rgba(128,128,0,0.5)';
        }
        if (numOverflowEntriesToTheRight > 0) {
          ctx.beginPath();
          ctx.moveTo(RIGHT_MARGIN - PAD - H / 2, y - H / 2);
          ctx.lineTo(RIGHT_MARGIN - PAD, y);
          ctx.lineTo(RIGHT_MARGIN - PAD - H / 2, y + H / 2);
          ctx.closePath();
          ctx.fill();
          ctx.textAlign = 'right';
          ctx.fillText(
              '+' + numOverflowEntriesToTheRight,
              RIGHT_MARGIN - 2 * PAD - H / 2, y);
        }
      }
      y = y + LINE_SPACING;
      numVisibleRequestsPerLine.push(numVisibleRequestsCurrLine);
      totalSecondsPerLine.push(totalSecsCurrLine);
    }

    // Draw highlighted sections for the histograms
    y = YBEGIN;
    for (j = 0; j < Intervals.length; j++) {
      if (IpmiVizHistHighlighted[Titles[j]] != undefined) {
        entry = HistogramThresholds[Titles[j]];
        const theSet = Array.from(IpmiVizHistHighlighted[Titles[j]]);
        for (i = 0; i < theSet.length; i++) {
          bidx = theSet[i];
          if (entry != undefined) {
            if (bidx < entry[0][0]) {
              if (bidx < 0) {
                bidx = 0;
              }
              ctx.fillStyle = 'rgba(0, 255, 0, 0.3)';
            } else if (bidx > entry[1][0]) {
              if (bidx > 1) {
                bidx = 1;
              }
              ctx.fillStyle = 'rgba(255,0,0,0.3)';
            } else {
              ctx.fillStyle = 'rgba(0,0,255,0.3)';
            }
          } else {
            ctx.fillStyle = 'rgba(0,0,255,0.3)';
          }
          dx = HISTOGRAM_X - HISTOGRAM_W / 2 + HISTOGRAM_W * bidx;

          r = HISTOGRAM_H * 0.17;
          ctx.beginPath();
          ctx.ellipse(dx, y, r, r, 0, 0, 3.14159 * 2);
          ctx.fill();
        }
      }
      y += LINE_SPACING;
    }

    // Render number of visible requests versus totals
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    let totalOccs = 0, totalSecs = 0;
    if (IsHighlighted()) {
      ctx.fillStyle = '#00F';
      ctx.fillText('Number/time in selection', 3, TEXT_Y0);
    } else {
      ctx.fillStyle = '#000';
      ctx.fillText('Number/time in viewport', 3, TEXT_Y0);
    }

    let timeDesc = '';
    ctx.textBaseline = 'middle';
    for (let i = 0; i < numVisibleRequestsPerLine.length; i++) {
      let y = YBEGIN + LINE_SPACING * i;
      let totalSeconds = totalSecondsPerLine[i];
      if (totalSeconds < 1) {
        timeDesc = (totalSeconds * 1000.0).toFixed(toFixedPrecision) + 'ms';
      } else {
        timeDesc = totalSeconds.toFixed(toFixedPrecision) + 's';
      }
      ctx.fillText('' + numVisibleRequestsPerLine[i] + ' / ' + timeDesc, 3, y);
      totalOccs += numVisibleRequestsPerLine[i];
      totalSecs += totalSeconds;
    }

    timeDesc = '';
    if (totalSecs < 1) {
      timeDesc = '' + (totalSecs * 1000).toFixed(toFixedPrecision) + 'ms';
    } else {
      timeDesc = '' + totalSecs.toFixed(toFixedPrecision) + 's';
    }

    ctx.fillText('Sum:', 3, y + 2 * LINE_SPACING);
    ctx.fillText('' + totalOccs + ' / ' + timeDesc, 3, y + 3 * LINE_SPACING);

    // Update highlighted requests
    if (IsHighlightDirty) {
      HighlightedRequests = currHighlightedReqs;
      IsHighlightDirty = false;
      OnHighlightedChanged(HighlightedRequests);
    }

    // Render highlight statistics
    if (IsHighlighted()) {
      ctx.fillStyle = 'rgba(128,128,255,0.5)';
      let x0 = MapXCoord(
          t0, LEFT_MARGIN, RIGHT_MARGIN, LowerBoundTime, UpperBoundTime);
      let x1 = MapXCoord(
          t1, LEFT_MARGIN, RIGHT_MARGIN, LowerBoundTime, UpperBoundTime);
      ctx.fillRect(x0, 0, x1 - x0, height);

      let label0 = '' + t0.toFixed(toFixedPrecision) + 's';
      let label1 = '' + t1.toFixed(toFixedPrecision) + 's';
      let width0 = ctx.measureText(label0).width;
      let width1 = ctx.measureText(label1).width;
      let dispWidth = x1 - x0;
      // Draw time marks outside or inside?
      ctx.fillStyle = '#0000FF';
      ctx.textBaseline = 'top';
      if (dispWidth > width0 + width1) {
        ctx.textAlign = 'left';
        ctx.fillText(label0, x0, LINE_SPACING + TEXT_Y0);
        ctx.textAlign = 'right';
        ctx.fillText(label1, x1, LINE_SPACING + TEXT_Y0);
      } else {
        ctx.textAlign = 'right';
        ctx.fillText(label0, x0, LINE_SPACING + TEXT_Y0);
        ctx.textAlign = 'left';
        ctx.fillText(label1, x1, LINE_SPACING + TEXT_Y0);
      }

      // This was calculated earlier
      ctx.textAlign = 'center';
      label1 = 'Duration: ' + (t1 - t0).toFixed(toFixedPrecision) + 's';
      ctx.fillText(label1, (x0 + x1) / 2, height - LINE_SPACING * 2);
    }

    // Hovering cursor
    if (MouseState.hovered == true && MouseState.hoveredSide == undefined) {
      ctx.beginPath();
      ctx.strokeStyle = '#0000FF';
      ctx.lineWidth = 1;
      if (IsHighlighted()) {
        ctx.moveTo(MouseState.x, 0);
        ctx.lineTo(MouseState.x, height);
      } else {
        ctx.moveTo(MouseState.x, LINE_SPACING * 2);
        ctx.lineTo(MouseState.x, height - LINE_SPACING * 2);
      }
      ctx.stroke();

      if (!IsHighlighted()) {
        let dispWidth = MouseState.x - LEFT_MARGIN;
        let label = '' +
            MouseXToTimestamp(MouseState.x).toFixed(toFixedPrecision) + 's';
        let width0 = ctx.measureText(label).width;
        ctx.fillStyle = '#0000FF';
        ctx.textBaseline = 'bottom';
        ctx.textAlign = 'center';
        ctx.fillText(label, MouseState.x, height - LINE_SPACING);
        ctx.textBaseline = 'top';
        ctx.fillText(label, MouseState.x, LINE_SPACING + TEXT_Y0);
      }
    }

    // Tooltip box next to hovered entry
    if (theHoveredReq != undefined) {
      const PAD = 2, DELTA_Y = 14;

      let labels = [];
      let netFn = theHoveredReq[0];
      let cmd = theHoveredReq[1];
      let t0 = theHoveredInterval[0];
      let t1 = theHoveredInterval[1];

      labels.push('Netfn and CMD : (' + netFn + ', ' + cmd + ')');
      let key = netFn + ', ' + cmd;

      if (NetFnCmdToDescription[key] != undefined) {
        labels.push('Description   : ' + NetFnCmdToDescription[key]);
      }
      let req = theHoveredReq[4];
      labels.push('Request Data  : ' + req.length + ' bytes');
      if (req.length > 0) {
        labels.push('Hex   : ' + ToHexString(req, '', ' '));
        labels.push('ASCII : ' + ToASCIIString(req));
      }
      let resp = theHoveredReq[5];
      labels.push('Response Data : ' + theHoveredReq[5].length + ' bytes');
      if (resp.length > 0) {
        labels.push('Hex   : ' + ToHexString(resp, '', ' '));
        labels.push('ASCII : ' + ToASCIIString(resp));
      }
      labels.push('Start         : ' + t0.toFixed(toFixedPrecision) + 's');
      labels.push('End           : ' + t1.toFixed(toFixedPrecision) + 's');
      labels.push('Duration      : ' + ((t1 - t0) * 1000).toFixed(3) + 'ms');


      let w = 1, h = LINE_SPACING * labels.length + 2 * PAD;
      for (let i = 0; i < labels.length; i++) {
        w = Math.max(w, ctx.measureText(labels[i]).width);
      }
      let dy = MouseState.y + DELTA_Y;
      let dx = MouseState.x;
      if (RIGHT_MARGIN - dx < w) dx -= (w + 2 * PAD);

      ctx.fillStyle = 'rgba(0,0,0,0.5)';
      ctx.fillRect(dx, dy, w + 2 * PAD, h);

      ctx.textAlign = 'left';
      ctx.textBaseline = 'middle';
      ctx.fillStyle = '#FFFFFF';
      for (let i = 0; i < labels.length; i++) {
        ctx.fillText(
            labels[i], dx + PAD,
            dy + PAD + i * LINE_SPACING + LINE_SPACING / 2);
      }
    }
  }
}


window.addEventListener('keydown', function() {
  if (event.keyCode == 37) {  // Left Arrow
    ipmi_timeline_view.CurrDeltaX = -0.004;
    dbus_timeline_view.CurrDeltaX = -0.004;
  } else if (event.keyCode == 39) {  // Right arrow
    ipmi_timeline_view.CurrDeltaX = 0.004;
    dbus_timeline_view.CurrDeltaX = 0.004;
  } else if (event.keyCode == 16) {  // Shift
    ipmi_timeline_view.CurrShiftFlag = true;
    dbus_timeline_view.CurrShiftFlag = true;
  } else if (event.keyCode == 38) {  // Up arrow
    ipmi_timeline_view.CurrDeltaZoom = 0.01;
    dbus_timeline_view.CurrDeltaZoom = 0.01;
  } else if (event.keyCode == 40) {  // Down arrow
    ipmi_timeline_view.CurrDeltaZoom = -0.01;
    dbus_timeline_view.CurrDeltaZoom = -0.01;
  }
});

window.addEventListener('keyup', function() {
  if (event.keyCode == 37 || event.keyCode == 39) {
    ipmi_timeline_view.CurrDeltaX = 0;
    dbus_timeline_view.CurrDeltaX = 0;
  } else if (event.keyCode == 16) {
    ipmi_timeline_view.CurrShiftFlag = false;
    dbus_timeline_view.CurrShiftFlag = false;
  } else if (event.keyCode == 38 || event.keyCode == 40) {
    ipmi_timeline_view.CurrDeltaZoom = 0;
    dbus_timeline_view.CurrDeltaZoom = 0;
  }
});

function MouseXToTimestamp(x) {
  let ret = (x - LEFT_MARGIN) / (RIGHT_MARGIN - LEFT_MARGIN) *
          (UpperBoundTime - LowerBoundTime) +
      LowerBoundTime;
  ret = Math.max(ret, LowerBoundTime);
  ret = Math.min(ret, UpperBoundTime);
  return ret;
}

let HighlightedRegion = {t0: -999, t1: -999};

function IsHighlighted() {
  return (HighlightedRegion.t0 != -999 && HighlightedRegion.t1 != -999);
}

function Unhighlight() {
  HighlightedRegion.t0 = -999;
  HighlightedRegion.t1 = -999;
}

function UnhighlightIfEmpty() {
  if (HighlightedRegion.t0 == HighlightedRegion.t1) {
    Unhighlight();
    return true;
  }
  return false;
}

let MouseState = {
  hovered: true,
  pressed: false,
  x: 0,
  y: 0,
  hoveredLineIndex: -999,
  hoveredSide: undefined
};
let Canvas = document.getElementById('my_canvas_ipmi');

Canvas.onmousemove = function(event) {
  const v = ipmi_timeline_view;
  v.MouseState.x = event.pageX - this.offsetLeft;
  v.MouseState.y = event.pageY - this.offsetTop;
  if (v.MouseState.pressed == true) {  // Update highlighted area
    v.HighlightedRegion.t1 = v.MouseXToTimestamp(v.MouseState.x);
  }
  v.OnMouseMove();
  v.IsCanvasDirty = true;

  v.linked_views.forEach(function(u) {
    u.MouseState.x = event.pageX - Canvas.offsetLeft;
    u.MouseState.y = 0;                  // Do not highlight any entry
    if (u.MouseState.pressed == true) {  // Update highlighted area
      u.HighlightedRegion.t1 = u.MouseXToTimestamp(u.MouseState.x);
    }
    u.OnMouseMove();
    u.IsCanvasDirty = true;
  });
};

function OnMouseMove() {
  const PAD = 2;
  if (MouseState.x < LEFT_MARGIN)
    MouseState.hovered = false;
  else if (MouseState.x > RIGHT_MARGIN)
    MouseState.hovered = false;
  else
    MouseState.hovered = true;
  IsCanvasDirty = true;
  let lineIndex = Math.floor((MouseState.y - YBEGIN + TEXT_Y0) / LINE_SPACING);
  MouseState.hoveredSide = undefined;
  MouseState.hoveredLineIndex = -999;
  if (lineIndex < Intervals.length) {
    MouseState.hoveredLineIndex = lineIndex;
    if (MouseState.x <= PAD + LINE_SPACING / 2 + LEFT_MARGIN &&
        MouseState.x >= PAD + LEFT_MARGIN) {
      MouseState.hoveredSide = 'left';
      IsCanvasDirty = true;
    } else if (
        MouseState.x <= RIGHT_MARGIN - PAD &&
        MouseState.x >= RIGHT_MARGIN - PAD - LINE_SPACING / 2) {
      MouseState.hoveredSide = 'right';
      IsCanvasDirty = true;
    }
  }
}

Canvas.onmouseover = function() {
  OnMouseMove();
  ipmi_timeline_view.OnMouseMove();
};

Canvas.onmouseleave = function() {
  MouseState.hovered = false;
  IsCanvasDirty = true;
  ipmi_timeline_view.OnMouseLeave();
  dbus_timeline_view.OnMouseLeave();
};

Canvas.onmousedown = function(event) {
  if (event.button == 0) {  // Left mouse button
    ipmi_timeline_view.OnMouseDown();
  }
};

Canvas.onmouseup = function(event) {
  if (event.button == 0) {
    ipmi_timeline_view.OnMouseUp();
    // page-specific, not view-specific
    let hint = document.getElementById('highlight_hint');
    if (ipmi_timeline_view.UnhighlightIfEmpty()) {
      hint.style.display = 'none';
    } else {
      hint.style.display = 'block';
    }
  }
};

Canvas.onwheel = function(event) {
  event.preventDefault();
  const v = ipmi_timeline_view;
  if (v.IsMouseOverTimeline()) {
    let dz = 0;
    if (event.deltaY > 0) {  // Scroll down, zoom out
      dz = -0.3;
    } else if (event.deltaY < 0) {  // Scroll up, zoom in
      dz = 0.3;
    }
    v.Zoom(dz, v.MouseXToTimestamp(v.MouseState.x));
  } else {
    if (event.deltaY > 0) {
      v.ScrollY(1);
    } else if (event.deltaY < 0) {
      v.ScrollY(-1);
    }
  }
};

// This function is not specific to TimelineView so putting it here
function OnHighlightedChanged(reqs) {
  let x = document.getElementById('ipmi_replay');
  let i = document.getElementById('ipmi_replay_output');
  let cnt = document.getElementById('highlight_count');
  cnt.innerHTML = '' + reqs.length;
  i.style.display = 'none';
  if (reqs.length > 0) {
    x.style.display = 'block';
  } else
    x.style.display = 'none';
  let o = document.getElementById('ipmi_replay_output');
  o.style.display = 'none';
  o.textContent = '';
}

function ToHexString(bytes, prefix, sep) {
  let ret = '';
  for (let i = 0; i < bytes.length; i++) {
    if (i > 0) {
      ret += sep;
    }
    ret += prefix + bytes[i].toString(16);
  }
  return ret;
}

function ToASCIIString(bytes) {
  ret = '';
  for (let i = 0; i < bytes.length; i++) {
    ret = ret + String.fromCharCode(bytes[i]);
  }
  return ret;
}

function ShowReplayOutputs(x, ncols) {
  let o = document.getElementById('ipmi_replay_output');
  o.cols = ncols;
  o.style.display = 'block';
  o.textContent = x;
}

function GenerateIPMIToolIndividualCommandReplay(reqs) {
  let x = '';
  for (let i = 0; i < reqs.length; i++) {
    let req = reqs[i];
    // [0]: NetFN, [1]: cmd, [4]: payload
    // NetFN and cmd are DECIMAL while payload is HEXADECIMAL.
    x = x + 'ipmitool raw ' + req[0] + ' ' + req[1] + ' ' +
        ToHexString(req[4], '0x', ' ') + '\n';
  }
  ShowReplayOutputs(x, 80);
}

function GenerateIPMIToolExecListReplay(reqs) {
  console.log(reqs.length);
  let x = '';
  for (let i = 0; i < reqs.length; i++) {
    let req = reqs[i];
    x = x + 'raw ' +
        ToHexString([req[0]].concat([req[1]]).concat(req[4]), '0x', ' ') + '\n';
  }
  ShowReplayOutputs(x, 80);
}

function GenerateBusctlReplayLegacyInterface(reqs) {
  console.log(reqs.length);
  let serial = 0;
  let x = '';
  for (let i = 0; i < reqs.length; i++) {
    let req = reqs[i];
    x = x +
        'busctl --system emit  /org/openbmc/HostIpmi/1 org.openbmc.HostIpmi ReceivedMessage yyyyay ';
    x = x + serial + ' ' + req[0] + ' 0 ' + req[1] + ' ' + req[4].length + ' ' +
        ToHexString(req[4], '0x', ' ') + '\n';
    serial = (serial + 1) % 256;
  }
  ShowReplayOutputs(x, 120);
}

function GenerateBusctlReplayNewInterface(reqs) {
  console.log(reqs.length);
  let x = '';
  for (let i = 0; i < reqs.length; i++) {
    let req = reqs[i];
    x = x +
        'busctl --system call xyz.openbmc_project.Ipmi.Host /xyz/openbmc_project/Ipmi xyz.openbmc_project.Ipmi.Server execute yyyaya{sv} ';
    x = x + req[0] + ' 0 ' + req[1] + ' ' + req[4].length + ' ' +
        ToHexString(req[4], '0x', ' ');
    +' 0\n';
  }
  ShowReplayOutputs(x, 150);
}
