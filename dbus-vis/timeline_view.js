const { TouchBarScrubber } = require("electron");

// Default range: 0 to 300s, shared between both views
var RANGE_LEFT_INIT = 0;
var RANGE_RIGHT_INIT = 300;

// Global timeline start
var g_StartingSec = undefined;

function ShouldShowDebugInfo() {
  if (g_cb_debug_info.checked) return true;
  else return false;
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

// A TimelineView contains data that has already gone through
// the Layout step and is ready for showing
class TimelineView {
  constructor() {
    this.Intervals = [];
    this.Titles = [];  // { "header":true|false, "title":string, "intervals_idxes":[int] } 
    this.Heights = [];  // Visual height for each line
    this.HeaderCollapsed = {};
    this.TitleProperties = []; // [Visual height, Is Header]
    this.LowerBoundTime = RANGE_LEFT_INIT;
    this.UpperBoundTime = RANGE_RIGHT_INIT;
    this.LowerBoundTimeTarget = this.LowerBoundTime;
    this.UpperBoundTimeTarget = this.UpperBoundTime;
    this.LastTimeLowerBound = 0;
    this.LastTimeUpperBound = 0;
    this.IsCanvasDirty = true;
    this.IsHighlightDirty = true;
    this.IsAnimating = false;
    this.IpmiVizHistogramImageData = {};
    this.IpmiVizHistHighlighted = {};
    this.HighlightedRequests = [];
    this.Canvas = undefined;
    this.TitleDispLengthLimit = 32;  // display this many chars for title
    this.IsTimeDistributionEnabled = false;
    this.AccentColor = '#000';
    this.CurrentFileName = '';
    this.VisualLineStartIdx = 0;

    // For connecting to the data model
    this.GroupBy = [];
    this.GroupByStr = '';

    // For keyboard navigation
    this.CurrDeltaX = 0;
    this.CurrDeltaZoom = 0;
    this.CurrShiftFlag = false;
    this.MouseState = {
      hovered: true,
      pressed: false,
      x: 0,
      y: 0,
      hoveredVisibleLineIndex: -999,
      hoveredSide: undefined,  // 'left', 'right', 'scroll', 'timeline'
      drag_begin_title_start_idx: undefined,
      drag_begin_y: undefined,
      IsDraggingScrollBar: function() {
        return (this.drag_begin_y != undefined);
      },
      EndDragScrollBar: function() {
        this.drag_begin_y = undefined;
        this.drag_begin_title_start_idx = undefined;
      },
      IsHoveredOverHorizontalScrollbar: function() {
        if (this.hoveredSide == "top_horizontal_scrollbar") return true;
        else if (this.hoveredSide == "bottom_horizontal_scrollbar") return true;
        else return false;
      }
    };
    this.ScrollBarState = {
      y0: undefined,
      y1: undefined,
    };
    this.HighlightedRegion = {t0: -999, t1: -999};

    // The linked view will move and zoom with this view
    this.linked_views = [];
  }

  // Performs layout operation, move overlapping intervals to different
  // lines
  LayoutForOverlappingIntervals() {
    this.Heights = [];
    const MAX_STACK = 10; // Stack level limit: 10, arbitrarily chosen
    
    for (let i=0; i<this.Titles.length; i++) {
      let last_x = {};
      let ymax = 0;

      const title_data = this.Titles[i];

      const intervals_idxes = title_data.intervals_idxes;

      // TODO: What happens if there are > 1
      if (title_data.header == false) {
        const line = this.Intervals[intervals_idxes[0]];

        for (let j=0; j<line.length; j++) {
          const entry = line[j];
          let y = 0;
          for (; y<MAX_STACK; y++) {
            if (!(y in last_x)) { break; }
            if (last_x[y] <= entry[0]) {
              break;
            }
          }
          
          const end_time = entry[1];
          if (end_time != undefined && !isNaN(end_time)) {
            last_x[y] = end_time;
          } else {
            last_x[y] = entry[0];
          }
          entry[4] = y;
          ymax = Math.max(y, ymax);
        }
      } else if (intervals_idxes.length == 0) {
        // Don't do anything, set height to 1
      }
      this.Heights.push(ymax+1);
    }
  }

  TotalVisualHeight() {
    let ret = 0;
    this.Heights.forEach((h) => {
      ret += h;
    })
    return ret;
  }

  // Returns [Index, Offset]
  VisualLineIndexToDataLineIndex(x) {
    if (this.Heights.length < 1) return undefined;
    let lb = 0, ub = this.Heights[0]-1;
    for (let i=0; i<this.Heights.length; i++) {
      ub = lb + this.Heights[i] - 1;
      if (lb <= x && ub >= x) {
        return [i, x-lb];
      }
      lb = ub+1;
    }
    return undefined;
  }

  IsEmpty() {
    return (this.Intervals.length < 1);
  }

  GetTitleWidthLimit() {
    if (this.IsTimeDistributionEnabled == true) {
      return 32;
    } else {
      return 64;
    }
  }

  ToLines(t, limit) {
    let ret = [];
    for (let i = 0; i < t.length; i += limit) {
      let j = Math.min(i + limit, t.length);
      ret.push(t.substr(i, j));
    }
    return ret;
  }

  Zoom(dz, mid = undefined, iter = 1) {
    if (this.CurrShiftFlag) dz *= 2;
    if (dz != 0) {
      if (mid == undefined) {
        mid = (this.LowerBoundTime + this.UpperBoundTime) / 2;
      }
      this.LowerBoundTime = mid - (mid - this.LowerBoundTime) * (1 - dz);
      this.UpperBoundTime = mid + (this.UpperBoundTime - mid) * (1 - dz);
      this.IsCanvasDirty = true;
      this.IsAnimating = false;
    }

    if (iter > 0) {
      this.linked_views.forEach(function(v) {
        v.Zoom(dz, mid, iter - 1);
      });
    }
  }

  BeginZoomAnimation(dz, mid = undefined, iter = 1) {
    if (mid == undefined) {
      mid = (this.LowerBoundTime + this.UpperBoundTime) / 2;
    }
    this.LowerBoundTimeTarget = mid - (mid - this.LowerBoundTime) * (1 - dz);
    this.UpperBoundTimeTarget = mid + (this.UpperBoundTime - mid) * (1 - dz);
    this.IsCanvasDirty = true;
    this.IsAnimating = true;

    if (iter > 0) {
      this.linked_views.forEach(function(v) {
        v.BeginZoomAnimation(dz, mid, iter - 1);
      });
    }
  }

  BeginPanScreenAnimaton(delta_screens, iter = 1) {
    let deltat = (this.UpperBoundTime - this.LowerBoundTime) * delta_screens;
    this.BeginSetBoundaryAnimation(
        this.LowerBoundTime + deltat, this.UpperBoundTime + deltat);

    if (iter > 0) {
      this.linked_views.forEach(function(v) {
        v.BeginPanScreenAnimaton(delta_screens, iter - 1);
      });
    }
  }

  BeginSetBoundaryAnimation(lt, rt, iter = 1) {
    this.IsAnimating = true;
    this.LowerBoundTimeTarget = lt;
    this.UpperBoundTimeTarget = rt;

    if (iter > 0) {
      this.linked_views.forEach(function(v) {
        v.BeginSetBoundaryAnimation(lt, rt, iter - 1);
      });
    }
  }

  BeginWarpToRequestAnimation(req, iter = 1) {
    let mid_new = (req[0] + req[1]) / 2;
    let mid = (this.LowerBoundTime + this.UpperBoundTime) / 2;
    let lt = this.LowerBoundTime + (mid_new - mid);
    let rt = this.UpperBoundTime + (mid_new - mid);
    this.BeginSetBoundaryAnimation(lt, rt, 0);

    this.linked_views.forEach(function(v) {
      v.BeginSetBoundaryAnimation(lt, rt, 0);
    });
  }

  UpdateAnimation() {
    const EPS = 1e-3;
    if (Math.abs(this.LowerBoundTime - this.LowerBoundTimeTarget) < EPS &&
        Math.abs(this.UpperBoundTime - this.UpperBoundTimeTarget) < EPS) {
      this.LowerBoundTime = this.LowerBoundTimeTarget;
      this.UpperBoundTime = this.UpperBoundTimeTarget;
      this.IsAnimating = false;
    }
    if (this.IsAnimating) {
      let t = 0.80;
      this.LowerBoundTime =
          this.LowerBoundTime * t + this.LowerBoundTimeTarget * (1 - t);
      this.UpperBoundTime =
          this.UpperBoundTime * t + this.UpperBoundTimeTarget * (1 - t);
      this.IsCanvasDirty = true;
    }
  }

  IsHighlighted() {
    return (
        this.HighlightedRegion.t0 != -999 && this.HighlightedRegion.t1 != -999);
  }

  RenderHistogram(ctx, key, xMid, yMid) {
    if (GetHistoryHistogram()[key] == undefined) {
      return;
    }
    if (this.IpmiVizHistogramImageData[key] == undefined) {
      return;
    }
    let hist = GetHistoryHistogram()[key];
    ctx.putImageData(
        this.IpmiVizHistogramImageData[key], xMid - HISTOGRAM_W / 2,
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

  IsMouseOverTimeline() {
    return this.MouseState.x > LEFT_MARGIN;
  }

  MouseXToTimestamp(x) {
    let ret = (x - LEFT_MARGIN) / (RIGHT_MARGIN - LEFT_MARGIN) *
            (this.UpperBoundTime - this.LowerBoundTime) +
        this.LowerBoundTime;
    ret = Math.max(ret, this.LowerBoundTime);
    ret = Math.min(ret, this.UpperBoundTime);
    return ret;
  }

  Unhighlight() {
    this.HighlightedRegion.t0 = -999;
    this.HighlightedRegion.t1 = -999;
  }

  OnMouseMove() {
    // Drag gestures
    if (this.MouseState.pressed == true) {
      const h = this.MouseState.hoveredSide;
      if (h == 'timeline') {
        // Update highlighted area
        this.HighlightedRegion.t1 =
          this.MouseXToTimestamp(this.MouseState.x);
      }
    }

    const PAD = 2;
    if (this.MouseState.x < LEFT_MARGIN)
      this.MouseState.hovered = false;
    else if (this.MouseState.x > RIGHT_MARGIN)
      this.MouseState.hovered = false;
    else
      this.MouseState.hovered = true;

    this.IsCanvasDirty = true;
    let lineIndex =
        Math.floor((this.MouseState.y - YBEGIN + TEXT_Y0) / LINE_SPACING);

    if (this.MouseState.x <= 0 ||
        this.MouseState.x >= RIGHT_MARGIN) {
      lineIndex = undefined;
    }

    const old_hoveredSide = this.MouseState.hoveredSide;

    // Left/right overflow markers or time axis drag
    this.MouseState.hoveredVisibleLineIndex = -999;
    if (this.MouseState.hoveredSide != "scrollbar" &&
        this.MouseState.pressed == false) {
      if (lineIndex != undefined) {
        this.MouseState.hoveredVisibleLineIndex = lineIndex;

        let should_hide_cursor = false;  // Should we hide the vertical cursor for linked views?

        if (this.MouseState.x <= PAD + LINE_SPACING / 2 + LEFT_MARGIN &&
            this.MouseState.x >= PAD + LEFT_MARGIN) {
          this.MouseState.hoveredSide = 'left';
          this.IsCanvasDirty = true;
        } else if (
            this.MouseState.x <= RIGHT_MARGIN - PAD &&
            this.MouseState.x >= RIGHT_MARGIN - PAD - LINE_SPACING / 2) {
          this.MouseState.hoveredSide = 'right';
          this.IsCanvasDirty = true;
        } else if (this.MouseState.x >= PAD + LEFT_MARGIN &&
                   this.MouseState.y <= TOP_HORIZONTAL_SCROLLBAR_HEIGHT &&
                   this.MouseState.y >  0) {
          this.MouseState.hoveredVisibleLineIndex = undefined;
          this.MouseState.hoveredSide = 'top_horizontal_scrollbar';
        } else if (this.MouseState.x >= PAD + LEFT_MARGIN &&
                   this.MouseState.y >= this.Canvas.height - BOTTOM_HORIZONTAL_SCROLLBAR_HEIGHT &&
                   this.MouseState.y <= this.Canvas.height) {
          this.MouseState.hoveredVisibleLineIndex = undefined;
          this.MouseState.hoveredSide = 'bottom_horizontal_scrollbar';
        } else {
          this.MouseState.hoveredSide = undefined;
        }
      }
    }

    // During a dragging session
    if (this.MouseState.pressed == true) {

      if (this.MouseState.hoveredSide == "top_horizontal_scrollbar" ||
          this.MouseState.hoveredSide == "bottom_horizontal_scrollbar") {
        const sec_per_px = (this.MouseState.begin_UpperBoundTime - this.MouseState.begin_LowerBoundTime) / (RIGHT_MARGIN - LEFT_MARGIN);
        const pan_secs = (this.MouseState.x - this.MouseState.begin_drag_x) * sec_per_px;

        const new_lb = this.MouseState.begin_LowerBoundTime - pan_secs;
        const new_ub = this.MouseState.begin_UpperBoundTime - pan_secs;
        this.LowerBoundTime = new_lb;
        this.UpperBoundTime = new_ub;

        // Sync to all other views
        this.linked_views.forEach((v) => {
          v.LowerBoundTime = new_lb; v.UpperBoundTime = new_ub;
        })
      }

      const tvh = this.TotalVisualHeight();
      if (this.MouseState.hoveredSide == 'scrollbar') {
        const diff_y = this.MouseState.y - this.MouseState.drag_begin_y;
        const diff_title_idx = tvh * diff_y / this.Canvas.height;
        let new_title_start_idx = this.MouseState.drag_begin_title_start_idx + parseInt(diff_title_idx);
        if (new_title_start_idx < 0) { new_title_start_idx = 0; }
        else if (new_title_start_idx >= tvh) {
          new_title_start_idx = tvh - 1;
        }
        this.VisualLineStartIdx = new_title_start_idx;
      }
    }
  }

  OnMouseLeave() {
    // When dragging the scroll bar, allow mouse to temporarily leave the element since we only
    // care about delta Y
    if (this.MouseState.hoveredSide == 'scrollbar') {
      
    } else {
      this.MouseState.hovered = false;
      this.MouseState.hoveredSide = undefined;
      this.IsCanvasDirty = true;
      this.MouseState.hoveredVisibleLineIndex = undefined;
      this.MouseState.y = undefined;
      this.MouseState.x = undefined;
    }
  }

  // Assume event.button is zero (left mouse button)
  OnMouseDown(iter = 1) {
    // If hovering over an overflowing triangle, warp to the nearest overflowed
    //     request on that line
    if (this.MouseState.hoveredVisibleLineIndex >= 0 &&
        this.MouseState.hoveredVisibleLineIndex < this.Intervals.length &&
        this.MouseState.hoveredSide != undefined) {
      const x = this.VisualLineIndexToDataLineIndex(this.MouseState.hoveredVisibleLineIndex);
      if (x == undefined) return;
      const line = this.Intervals[x[0]];
      if (this.MouseState.hoveredSide == 'left') {
        for (let i = line.length - 1; i >= 0; i--) {
          if (line[i][1] <= this.LowerBoundTime) {
            this.BeginWarpToRequestAnimation(line[i]);
            // TODO: pass timeline X to linked view
            break;
          }
        }
      } else if (this.MouseState.hoveredSide == 'right') {
        for (let i = 0; i < line.length; i++) {
          if (line[i][0] >= this.UpperBoundTime) {
            // TODO: pass timeline X to linked view
            this.BeginWarpToRequestAnimation(line[i]);
            break;
          }
        }
      }
    }

    let tx = this.MouseXToTimestamp(this.MouseState.x);
    let t0 = Math.min(this.HighlightedRegion.t0, this.HighlightedRegion.t1),
        t1 = Math.max(this.HighlightedRegion.t0, this.HighlightedRegion.t1);
    if (this.MouseState.x > LEFT_MARGIN) {

      // If clicking on the horizontal scroll bar, start panning the viewport
      if (this.MouseState.hoveredSide == "top_horizontal_scrollbar" ||
          this.MouseState.hoveredSide == "bottom_horizontal_scrollbar") {
        this.MouseState.pressed = true;
        this.MouseState.begin_drag_x = this.MouseState.x;
        this.MouseState.begin_LowerBoundTime = this.LowerBoundTime;
        this.MouseState.begin_UpperBoundTime = this.UpperBoundTime;
      } else if (tx >= t0 && tx <= t1) {
        // If clicking inside highlighted area, zoom around the area
        this.BeginSetBoundaryAnimation(t0, t1);
        this.Unhighlight();
        this.IsCanvasDirty = true;

        this.linked_views.forEach(function(v) {
          v.BeginSetBoundaryAnimation(t0, t1, 0);
          v.Unhighlight();
          v.IsCanvasDirty = false;
        });
      } else {  // If in the timeline area, start a new dragging action
        this.MouseState.hoveredSide = 'timeline';
        this.MouseState.pressed = true;
        this.HighlightedRegion.t0 = this.MouseXToTimestamp(this.MouseState.x);
        this.HighlightedRegion.t1 = this.HighlightedRegion.t0;
        this.IsCanvasDirty = true;
      }
    } else if (this.MouseState.x < SCROLL_BAR_WIDTH) {  // Todo: draagging the scroll bar
      const THRESH = 4;
      if (this.MouseState.y >= this.ScrollBarState.y0 - THRESH &&
          this.MouseState.y <= this.ScrollBarState.y1 + THRESH) {
        this.MouseState.pressed = true;
        this.MouseState.drag_begin_y = this.MouseState.y;
        this.MouseState.drag_begin_title_start_idx = this.VisualLineStartIdx;
        this.MouseState.hoveredSide = 'scrollbar';
      }
    }

    // Collapse or expand a "header"
    if (this.MouseState.x < LEFT_MARGIN &&
        this.MouseState.hoveredVisibleLineIndex != undefined) {
      const x = this.VisualLineIndexToDataLineIndex(this.VisualLineStartIdx + this.MouseState.hoveredVisibleLineIndex);
      if (x != undefined) {
        const tidx = x[0];
        if (this.Titles[tidx] != undefined && this.Titles[tidx].header == true) {

          // Currently, only DBus pane supports column headers, so we can hard-code the DBus re-group function (rather than to figure out which pane we're in)
          this.HeaderCollapsed[this.Titles[tidx].title] = !(this.HeaderCollapsed[this.Titles[tidx].title]);
          OnGroupByConditionChanged_DBus();
        }
      }
    }
  }

  // Assume event.button == 0 (left mouse button)
  OnMouseUp() {
    this.MouseState.EndDragScrollBar();
    this.MouseState.pressed = false;
    this.IsCanvasDirty = true;
    this.UnhighlightIfEmpty();
    this.IsHighlightDirty = true;
    this.MouseState.hoveredSide = undefined;

    // If highlighted area changed, update the info panel
    UpdateHighlightedMessagesInfoPanel();
  }

  UnhighlightIfEmpty() {
    if (this.HighlightedRegion.t0 == this.HighlightedRegion.t1) {
      this.Unhighlight();
      this.IsCanvasDirty = true;
      return true;
    } else
      return false;
  }

  OnMouseWheel(event) {
    event.preventDefault();
    const v = this;

    let is_mouse_on_horizontal_scrollbar = false;
    if (this.MouseState.y > 0 && this.MouseState.y < TOP_HORIZONTAL_SCROLLBAR_HEIGHT)
      is_mouse_on_horizontal_scrollbar = true;
    if (this.MouseState.y > this.Canvas.height - BOTTOM_HORIZONTAL_SCROLLBAR_HEIGHT &&
        this.MouseState.y < this.Canvas.height)
      is_mouse_on_horizontal_scrollbar = true;

    if (/*v.IsMouseOverTimeline()*/ is_mouse_on_horizontal_scrollbar) {
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
  }

  ScrollY(delta) {
    this.VisualLineStartIdx += delta;
    if (this.VisualLineStartIdx < 0) {
      this.VisualLineStartIdx = 0;
    } else if (this.VisualLineStartIdx >= this.TotalVisualHeight()) {
      this.VisualLineStartIdx = this.TotalVisualHeight() - 1;
    }
  }

  // This function is called in Render to draw a line of Intervals.
  // It is made into its own function for brevity in Render().
  // It depends on too much context so it doesn't look very clean though
  do_RenderIntervals(ctx, intervals_j, j, dy0, dy1, 
    data_line_idx, visual_line_offset_within_data_line,
    isAggregateSelection,
    vars,
    is_in_viewport) {
    // To reduce the number of draw calls while preserve the accuracy in
    // the visual presentation, combine rectangles that are within 1 pixel
    // into one
    let last_dx_begin = LEFT_MARGIN;
    let last_dx_end = LEFT_MARGIN; 

    for (let i = 0; i < intervals_j.length; i++) {
      let lb = intervals_j[i][0], ub = intervals_j[i][1];
      const yoffset = intervals_j[i][4];
      if (yoffset != visual_line_offset_within_data_line)
        continue;
      if (lb > ub)
        continue;  // Unmatched (only enter & no exit timestamp)

      let isHighlighted = false;
      let durationUsec =
          (intervals_j[i][1] - intervals_j[i][0]) * 1000000;
      let lbub = [lb, ub];
      if (this.IsHighlighted()) {
        if (IsIntersected(lbub, vars.highlightedInterval)) {
          vars.numIntersected++;
          isHighlighted = true;
          vars.currHighlightedReqs.push(intervals_j[i][2]);  // TODO: change the name to avoid confusion with HighlightedMessages
        }
      }

      if (ub < this.LowerBoundTime) {
        vars.numOverflowEntriesToTheLeft++;
        continue;
      }
      if (lb > this.UpperBoundTime) {
        vars.numOverflowEntriesToTheRight++;
        continue;
      }
      // Failed request
      if (ub == undefined && lb < this.UpperBoundTime) {
        vars.numOverflowEntriesToTheLeft++;
        continue;
      }

      let dx0 = MapXCoord(
              lb, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
              this.UpperBoundTime),
          dx1 = MapXCoord(
              ub, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
              this.UpperBoundTime);

      dx0 = Math.max(dx0, LEFT_MARGIN);
      dx1 = Math.min(dx1, RIGHT_MARGIN);
      let dw = Math.max(0, dx1 - dx0);

      if (isHighlighted && is_in_viewport) {
        ctx.fillStyle = 'rgba(128,128,255,0.5)';
        ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
      }

      let isCurrentReqHovered = false;
      // Intersect with mouse using pixel coordinates

      // When the mouse position is within 4 pixels distance from an entry, consider
      // the mouse to be over that entry and show the information popup
      const X_TOLERANCE = 4;

      if (vars.theHoveredReq == undefined &&
          IsIntersectedPixelCoords(
              [dx0 - X_TOLERANCE, dx0 + dw + X_TOLERANCE],
              [this.MouseState.x, this.MouseState.x]) &&
          IsIntersectedPixelCoords(
              [dy0, dy1], [this.MouseState.y, this.MouseState.y])) {
        ctx.fillStyle = 'rgba(255,255,0,0.5)';
        if (is_in_viewport) ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
        vars.theHoveredReq = intervals_j[i][2];
        vars.theHoveredInterval = intervals_j[i];
        isCurrentReqHovered = true;
      }

      ctx.lineWidth = 0.5;


      // If this request is taking too long/is quick enough, use red/green
      let entry = HistogramThresholds[this.Titles[data_line_idx].title];

      let isError = false;
      if (intervals_j[i][3] == 'error') {
        isError = true;
      }

      if (entry != undefined) {
        if (entry[0][1] != undefined && durationUsec < entry[0][1]) {
          ctx.strokeStyle = '#0F0';
        } else if (
            entry[1][1] != undefined && durationUsec > entry[1][1]) {
          ctx.strokeStyle = '#A00';
        } else {
          ctx.strokeStyle = '#000';
        }
      } else {
        ctx.strokeStyle = '#000';
      }

      const duration = intervals_j[i][1] - intervals_j[i][0];
      if (!isNaN(duration)) {
        if (is_in_viewport) {
          if (isError) {
            ctx.fillStyle = 'rgba(192, 128, 128, 0.8)';
            ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
            ctx.strokeStyle = 'rgba(192, 128, 128, 1)';
          } else {
            ctx.fillStyle = undefined;
            ctx.strokeStyle = '#000';
          }
        }

        // This keeps track of the current "cluster" of requests
        // that might visually overlap (i.e less than 1 pixel wide).
        // This can greatly reduce overdraw and keep render time under
        // a reasonable bound.
        if (!ShouldShowDebugInfo()) {
          if (dx0+dw - last_dx_begin > 1 ||
              i == intervals_j.length - 1) {
            if (is_in_viewport) {
              ctx.strokeRect(last_dx_begin, dy0, 
                /*dx0+dw-last_dx_begin*/
                last_dx_end - last_dx_begin, // At least 1 pixel wide
                dy1-dy0);
            }
            last_dx_begin = dx0;
          }
        } else {
          if (is_in_viewport) {
            ctx.strokeRect(dx0, dy0, dw, dy1 - dy0);
          }
        }
        last_dx_end = dx0 + dw;
        this.numVisibleRequests++;
      } else {
        // This entry has only a beginning and not an end
        // perhaps the original method call did not return
        if (is_in_viewport) {
          if (isCurrentReqHovered) {
            ctx.fillStyle = 'rgba(192, 192, 0, 0.8)';
          } else {
            ctx.fillStyle = 'rgba(255, 128, 128, 0.8)';
          }
          ctx.beginPath();
          ctx.arc(dx0, (dy0 + dy1) / 2, HISTOGRAM_H * 0.17, 0, 2 * Math.PI);
          ctx.fill();
        }
      }


      // Affects whether this req will be reflected in the aggregate info
      //     section
      if ((isAggregateSelection == false) ||
          (isAggregateSelection == true && isHighlighted == true)) {
        if (!isNaN(duration)) {
          vars.numVisibleRequestsCurrLine++;
          vars.totalSecsCurrLine += duration;
        } else {
          vars.numFailedRequestsCurrLine++;
        }

        // If a histogram exists for Titles[j], process the highlighted
        //     histogram buckets
        if (GetHistoryHistogram()[this.Titles[data_line_idx].title] != undefined) {
          let histogramEntry = GetHistoryHistogram()[this.Titles[data_line_idx].title];
          let bucketInterval = (histogramEntry[1] - histogramEntry[0]) /
              histogramEntry[2].length;
          let bucketIndex =
              Math.floor(
                  (durationUsec - histogramEntry[0]) / bucketInterval) /
              histogramEntry[2].length;

          if (this.IpmiVizHistHighlighted[this.Titles[data_line_idx].title] == undefined) {
            this.IpmiVizHistHighlighted[this.Titles[data_line_idx].title] = new Set();
          }
          let entry = this.IpmiVizHistHighlighted[this.Titles[data_line_idx].title];
          entry.add(bucketIndex);
        }
      }
    }  // end for (i=0 to interval_j.length-1)
    
    if (!ShouldShowDebugInfo()) {
      ctx.strokeRect(last_dx_begin, dy0, 
        /*dx0+dw-last_dx_begin*/
        last_dx_end - last_dx_begin, // At least 1 pixel wide
        dy1-dy0);
    }
  }

  // For the header:
  do_RenderHeader(ctx, header, j, dy0, dy1, 
    data_line_idx, visual_line_offset_within_data_line,
    isAggregateSelection,
    vars, is_in_viewport) {

    const dy = (dy0+dy1) / 2;
    ctx.fillStyle = "rgba(192,192,255, 1)";

    ctx.strokeStyle = "rgba(192,192,255, 1)"

    const title_text = header.title + " (" + header.intervals_idxes.length + ")";
    let skip_render = false;

    ctx.save();

    if (this.HeaderCollapsed[header.title] == false) {  // Expanded
      const x0 = LEFT_MARGIN - LINE_HEIGHT;
      if (is_in_viewport) {
        ctx.fillRect(0, dy-LINE_HEIGHT/2, x0, LINE_HEIGHT);

        ctx.beginPath();
        ctx.moveTo(x0, dy0);
        ctx.lineTo(x0, dy1);
        ctx.lineTo(x0 + LINE_HEIGHT, dy1);
        ctx.fill();
        ctx.closePath();

        ctx.beginPath();
        ctx.lineWidth = 1.5;
        ctx.moveTo(0, dy1);
        ctx.lineTo(RIGHT_MARGIN, dy1);
        ctx.stroke();
        ctx.closePath();

        ctx.fillStyle = '#003';
        ctx.textBaseline = 'center';
        ctx.textAlign = 'right';
        ctx.fillText(title_text, LEFT_MARGIN - LINE_HEIGHT, dy);
      }

      // Don't draw the timelines so visual clutter is reduced
      skip_render = true;
    } else {
      const x0 = LEFT_MARGIN - LINE_HEIGHT / 2;
      if (is_in_viewport) {
        ctx.fillRect(0, dy-LINE_HEIGHT/2, x0, LINE_HEIGHT);
        
        ctx.beginPath();
        ctx.lineWidth = 1.5;
        ctx.moveTo(x0, dy0);
        ctx.lineTo(x0 + LINE_HEIGHT/2, dy);
        ctx.lineTo(x0, dy1);
        ctx.closePath();
        ctx.fill();

        /*
        ctx.beginPath();
        ctx.moveTo(0, dy);
        ctx.lineTo(RIGHT_MARGIN, dy);
        ctx.stroke();
        ctx.closePath();
        */

        ctx.fillStyle = '#003';
        ctx.textBaseline = 'center';
        ctx.textAlign = 'right';
        ctx.fillText(title_text, LEFT_MARGIN - LINE_HEIGHT, dy);
      }
    }

    ctx.fillStyle = "rgba(160,120,255,0.8)";

    ctx.restore();

    // Draw the merged intervals
    // Similar to drawing the actual messages in do_Render(), but no collision detection against the mouse, and no hovering tooltip processing involved
    const merged_intervals = header.merged_intervals;
    let dxx0 = undefined, dxx1 = undefined;
    for (let i=0; i<merged_intervals.length; i++) {
      const lb = merged_intervals[i][0], ub = merged_intervals[i][1], weight = merged_intervals[i][2];
      let duration = ub-lb;
      let duration_usec = duration * 1000000;
      const lbub = [lb, ub];
      
      let isHighlighted = false;
      if (this.IsHighlighted()) {
        if (IsIntersected(lbub, vars.highlightedInterval)) {
          vars.numIntersected += weight;
          isHighlighted = true;
        }
      }

      if (ub < this.LowerBoundTime) continue;
      if (lb > this.UpperBoundTime) continue;

      // Render only if collapsed
      if (!skip_render) {
        let dx0 = MapXCoord(
          lb, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
          this.UpperBoundTime),
            dx1 = MapXCoord(
          ub, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
          this.UpperBoundTime);
        dx0 = Math.max(dx0, LEFT_MARGIN);
        dx1 = Math.min(dx1, RIGHT_MARGIN);
        let dw = Math.max(1, dx1 - dx0);  // At least 1 pixel wide during rendering

        // Draw this interval
        //ctx.fillRect(dx0, dy0, dw, dy1-dy0);
        if (dxx0 == undefined || dxx1 == undefined) {
          dxx0 = dx0;
        }

        const MERGE_THRESH = 0.5;  // Pixels

        let should_draw = true;
        if (dxx1 == undefined || dx0 < dxx1 + MERGE_THRESH) should_draw = false;
        if (i == merged_intervals.length - 1) {
          should_draw = true;
          dxx1 = dx1 + MERGE_THRESH;
        }

        if (should_draw) {
          //console.log(dxx0 + ", " + dy0 + ", " + (dx1-dxx0) + ", " + LINE_HEIGHT);
          if (is_in_viewport) {
            ctx.fillRect(dxx0, dy0, dxx1-dxx0, LINE_HEIGHT);
          }
          dxx0 = undefined; dxx1 = undefined;
        } else {
          // merge
          dxx1 = dx1 + MERGE_THRESH;
        }
      }

      if ((isAggregateSelection == false) ||
          (isAggregateSelection == true && isHighlighted == true)) {
        vars.totalSecsCurrLine += duration;
        vars.numVisibleRequestsCurrLine += weight;
      }
    }
  }

  Render(ctx) {
    // Wait for initialization
    if (this.Canvas == undefined) return;

    // Update
    let toFixedPrecision = 2;
    const extent = this.UpperBoundTime - this.LowerBoundTime;
    {
      if (extent < 0.1) {
        toFixedPrecision = 4;
      } else if (extent < 1) {
        toFixedPrecision = 3;
      }
    }

    let dx = this.CurrDeltaX;
    if (dx != 0) {
      if (this.CurrShiftFlag) dx *= 5;
      this.LowerBoundTime += dx * extent;
      this.UpperBoundTime += dx * extent;
      this.IsCanvasDirty = true;
    }

    // Hovered interval for display
    let theHoveredReq = undefined;
    let theHoveredInterval = undefined;
    let currHighlightedReqs = [];

    let dz = this.CurrDeltaZoom;
    this.Zoom(dz);
    this.UpdateAnimation();

    this.LastTimeLowerBound = this.LowerBoundTime;
    this.LastTimeUpperBound = this.UpperBoundTime;

    if (this.IsCanvasDirty) {
      this.IsCanvasDirty = false;
      // Shorthand for HighlightedRegion.t{0,1}
      let t0 = undefined, t1 = undefined;

      // Highlight
      let highlightedInterval = [];
      let numIntersected =
          0;  // How many requests intersect with highlighted area
      if (this.IsHighlighted()) {
        t0 = Math.min(this.HighlightedRegion.t0, this.HighlightedRegion.t1);
        t1 = Math.max(this.HighlightedRegion.t0, this.HighlightedRegion.t1);
        highlightedInterval = [t0, t1];
      }
      this.IpmiVizHistHighlighted = {};

      const width = this.Canvas.width;
      const height = this.Canvas.height;

      ctx.globalCompositeOperation = 'source-over';
      ctx.clearRect(0, 0, width, height);
      ctx.strokeStyle = '#000';
      ctx.fillStyle = '#000';
      ctx.lineWidth = 1;

      ctx.font = '12px Monospace';

      // Highlight current line
      if (this.MouseState.hoveredVisibleLineIndex != undefined) {
        const hv_lidx = this.MouseState.hoveredVisibleLineIndex + this.VisualLineStartIdx;
        if (hv_lidx >= 0 &&
            hv_lidx < this.Titles.length) {
          ctx.fillStyle = 'rgba(32,32,32,0.2)';
          let dy = YBEGIN + LINE_SPACING * this.MouseState.hoveredVisibleLineIndex -
              LINE_SPACING / 2;
          ctx.fillRect(0, dy, RIGHT_MARGIN, LINE_SPACING);
        }
      }

      // Draw highlighted background over time labels when the mouse is hovering over
      // the time axis
      ctx.fillStyle = "#FF9";
      if (this.MouseState.hoveredSide == "top_horizontal_scrollbar") {
        ctx.fillRect(LEFT_MARGIN, 0, RIGHT_MARGIN-LEFT_MARGIN, TOP_HORIZONTAL_SCROLLBAR_HEIGHT);
      } else if (this.MouseState.hoveredSide == "bottom_horizontal_scrollbar") {
        ctx.fillRect(LEFT_MARGIN, height-BOTTOM_HORIZONTAL_SCROLLBAR_HEIGHT, RIGHT_MARGIN-LEFT_MARGIN, BOTTOM_HORIZONTAL_SCROLLBAR_HEIGHT);
      }

      ctx.fillStyle = '#000';
      // Time marks at the beginning & end of the visible range
      ctx.textBaseline = 'bottom';
      ctx.textAlign = 'left';
      ctx.fillText(
          '' + this.LowerBoundTime.toFixed(toFixedPrecision) + 's',
          LEFT_MARGIN + 3, height);
      ctx.textAlign = 'end';
      ctx.fillText(
          '' + this.UpperBoundTime.toFixed(toFixedPrecision) + 's',
          RIGHT_MARGIN - 3, height);

      ctx.textBaseline = 'top';
      ctx.textAlign = 'left';
      ctx.fillText(
          '' + this.LowerBoundTime.toFixed(toFixedPrecision) + 's',
          LEFT_MARGIN + 3, TEXT_Y0);
      ctx.textAlign = 'right';
      ctx.fillText(
          '' + this.UpperBoundTime.toFixed(toFixedPrecision) + 's',
          RIGHT_MARGIN - 3, TEXT_Y0);

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
      if (this.GroupByStr.length > 0) {
        columnTitle = this.GroupByStr;
      }
      ctx.textAlign = 'right';
      ctx.textBaseline = 'top';
      // Split into lines
      {
        let lines = this.ToLines(columnTitle, this.TitleDispLengthLimit)
        for (let i = 0; i < lines.length; i++) {
          ctx.fillText(lines[i], LEFT_MARGIN - 3, 3 + i * LINE_HEIGHT);
        }
      }

      if (this.IsTimeDistributionEnabled) {
        // "Histogram" title
        ctx.fillStyle = '#000';
        ctx.textBaseline = 'top';
        ctx.textAlign = 'center';
        ctx.fillText('Time Distribution', HISTOGRAM_X, TEXT_Y0);

        ctx.textAlign = 'right'
        ctx.fillText('In dataset /', HISTOGRAM_X, TEXT_Y0 + LINE_SPACING - 2);

        ctx.fillStyle = '#00F';

        ctx.textAlign = 'left'
        if (this.IsHighlighted()) {
          ctx.fillText(
              ' In selection', HISTOGRAM_X, TEXT_Y0 + LINE_SPACING - 2);
        }
        else {
          ctx.fillText(' In viewport', HISTOGRAM_X, TEXT_Y0 + LINE_SPACING - 2);
        }
      }

      ctx.fillStyle = '#000';

      // Time Axis Breaks
      const breakWidths = [
        86400,  10800,  3600,    1800,    1200,   600,   300,   120,
        60,     30,     10,      5,       2,      1,     0.5,   0.2,
        0.1,    0.05,   0.02,    0.01,    0.005,  0.002, 0.001, 0.0005,
        0.0002, 0.0001, 0.00005, 0.00002, 0.00001
      ];
      const BreakDrawLimit = 1000;  // Only draw up to this many grid lines

      let bidx = 0;
      while (bidx < breakWidths.length &&
             breakWidths[bidx] > this.UpperBoundTime - this.LowerBoundTime) {
        bidx++;
      }
      let breakWidth = breakWidths[bidx + 1];
      if (bidx < breakWidths.length) {
        let t2 = 0;  // Cannot name as "t0" otherwise clash
        bidx = 0;
        while (bidx < breakWidths.length) {
          while (t2 + breakWidths[bidx] < this.LowerBoundTime) {
            t2 += breakWidths[bidx];
          }
          if (t2 + breakWidths[bidx] >= this.LowerBoundTime &&
              t2 + breakWidths[bidx] <= this.UpperBoundTime) {
            break;
          }
          bidx++;
        }
        let draw_count = 0;
        if (bidx < breakWidths.length) {
          for (; t2 < this.UpperBoundTime; t2 += breakWidth) {
            if (t2 > this.LowerBoundTime) {
              ctx.beginPath();
              let dx = MapXCoord(
                  t2, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
                  this.UpperBoundTime);
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

              draw_count++;
              if (draw_count > BreakDrawLimit) break;
            }
          }
        }
      }

      // Whether we aggregate selected requests or visible requests
      let isAggregateSelection = false;
      if (this.IsHighlighted()) isAggregateSelection = true;
      let numVisibleRequestsPerLine = {}; // DataLineIndex -> Count
      let numFailedRequestsPerLine = {};
      let totalSecondsPerLine = {};

      // Range of Titles that were displayed
      let title_start_idx = this.VisualLineStartIdx, title_end_idx = title_start_idx;

      const tvh = this.TotalVisualHeight();

      // This is used to handle Intervals that have overlapping entries
      let last_data_line_idx = -999;//this.VisualLineIndexToDataLineIndex(this.VisualLineStartIdx);

      // 'j' denotes a line index; if the viewport starts from the middle of an overlapping series of
      // lines, 'j' will be rewinded to the first one in the series to make the counts correct.
      let j0 = this.VisualLineStartIdx;
      while (j0 > 0 && this.VisualLineIndexToDataLineIndex(j0)[1] > 0) { j0--; }

      // If should_render is false, we're counting the entries outisde the viewport
      // If should_render is true, do the rendering
      let should_render = false;

      // 'j' then iterates over the "visual rows" that need to be displayed.
      // A "visual row" might be one of:
      // 1. a "header" line
      // 2. an actual row of data (in the Intervals variable)

      // 'j' needs to go PAST the viewport if the last row is overlapping and spans beyond the viewport.
      for (let j = j0; j < tvh; j++) {

        if (j >= this.VisualLineStartIdx) { should_render = true; }
        if (y > height) { should_render = false; }

        const tmp = this.VisualLineIndexToDataLineIndex(j);
        if (tmp == undefined) break;
        const data_line_idx = tmp[0];
        const visual_line_offset_within_data_line = tmp[1];
       
        const should_render_title = (data_line_idx != last_data_line_idx) ||
                                       (j == this.VisualLineStartIdx); // The first visible line should always be drawn
        last_data_line_idx = data_line_idx;

        if (should_render_title && data_line_idx != -999 && should_render) { // Only draw line title and histogram per data line index not visual line index
          ctx.textBaseline = 'middle';
          ctx.textAlign = 'right';
          let desc_width = 0;
          if (NetFnCmdToDescription[this.Titles[data_line_idx].title] != undefined) {
            let desc = ' (' + NetFnCmdToDescription[this.Titles[data_line_idx].title] + ')';
            desc_width = ctx.measureText(desc).width;
            ctx.fillStyle = '#888';  // Grey
            ctx.fillText(desc, LEFT_MARGIN - 3, y);
          }


          // Plot histogram
          if (this.IsTimeDistributionEnabled == true) {
            const t = this.Titles[data_line_idx].title;
            if (GetHistoryHistogram()[t] != undefined) {
              if (this.IpmiVizHistogramImageData[t] == undefined) {
                let tmp = RenderHistogramForImageData(ctx, t);
                this.IpmiVizHistogramImageData[t] = tmp[0];
                HistogramThresholds[t] = tmp[1];
              }
              this.RenderHistogram(ctx, t, HISTOGRAM_X, y);
              ctx.textAlignment = 'right';
            } else {
            }
          }

          // If is HEADER: do not draw here, darw in do_RenderHeader()
          if (this.Titles[data_line_idx].header == false) {
            ctx.textAlignment = 'right';
            ctx.textBaseline = 'middle';
            ctx.fillStyle = '#000000';  // Revert to Black
            ctx.strokeStyle = '#000000';
            let tj_draw = this.Titles[data_line_idx].title;
            const title_disp_length_limit = this.GetTitleWidthLimit();
            if (tj_draw != undefined && tj_draw.length > title_disp_length_limit) {
              tj_draw = tj_draw.substr(0, title_disp_length_limit) + '...'
            }
            ctx.fillText(tj_draw, LEFT_MARGIN - 3 - desc_width, y);
          }
        } else if (should_render_title && data_line_idx == -999) {
          continue;
        }

        let numOverflowEntriesToTheLeft = 0;  // #entries below the lower bound
        let numOverflowEntriesToTheRight =
            0;                               // #entries beyond the upper bound
        let numVisibleRequestsCurrLine = 0;  // #entries visible
        let totalSecsCurrLine = 0;           // Total duration in seconds
        let numFailedRequestsCurrLine = 0;

        const intervals_idxes = this.Titles[data_line_idx].intervals_idxes;
        
        let intervals_j = undefined;
        if (intervals_idxes.length == 1) {
          intervals_j = this.Intervals[intervals_idxes[0]];
        }

        // Draw the contents in the set of intervals
        // The drawing method depends on whether this data line is a header or not
        
        // Save the context for reference for the rendering routines
        let vars = {
          "theHoveredReq": theHoveredReq,
          "theHoveredInterval": theHoveredInterval,
          "numIntersected": numIntersected,
          "numOverflowEntriesToTheLeft": numOverflowEntriesToTheLeft,
          "numOverflowEntriesToTheRight": numOverflowEntriesToTheRight,
          "currHighlightedReqs": currHighlightedReqs,
          "totalSecondsPerLine": totalSecondsPerLine,
          "highlightedInterval": highlightedInterval,
          "numVisibleRequestsCurrLine": numVisibleRequestsCurrLine,
          "totalSecsCurrLine": totalSecsCurrLine,
        }  // Emulate a reference

        let dy0 = y - LINE_HEIGHT / 2, dy1 = y + LINE_HEIGHT / 2;
        if (this.Titles[data_line_idx].header == false) {
          if (intervals_j != undefined) {
            this.do_RenderIntervals(ctx, intervals_j, j, dy0, dy1,
              data_line_idx, visual_line_offset_within_data_line, isAggregateSelection, vars, should_render);
          }
        } else {
          this.do_RenderHeader(ctx, this.Titles[data_line_idx],
            j, dy0, dy1,
            data_line_idx, visual_line_offset_within_data_line, isAggregateSelection, vars, should_render);
        }

        // Update the context variables with updated values
        theHoveredReq = vars.theHoveredReq;
        theHoveredInterval = vars.theHoveredInterval;
        numIntersected = vars.numIntersected;
        numOverflowEntriesToTheLeft = vars.numOverflowEntriesToTheLeft;
        numOverflowEntriesToTheRight = vars.numOverflowEntriesToTheRight;
        currHighlightedReqs = vars.currHighlightedReqs;
        totalSecondsPerLine = vars.totalSecondsPerLine;
        highlightedInterval = vars.highlightedInterval;
        numVisibleRequestsCurrLine = vars.numVisibleRequestsCurrLine;
        totalSecsCurrLine = vars.totalSecsCurrLine;

        // Triangle markers for entries outside of the viewport
        {
          const PAD = 2, H = LINE_SPACING;
          if (this.MouseState.hoveredVisibleLineIndex + this.VisualLineStartIdx == data_line_idx &&
              this.MouseState.hoveredSide == 'left') {
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
                '+' + numOverflowEntriesToTheLeft,
                LEFT_MARGIN + 2 * PAD + H / 2, y);
          }

          if (this.MouseState.hoveredVisibleLineIndex + this.VisualLineStartIdx == j &&
              this.MouseState.hoveredSide == 'right') {
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

        if (should_render)
          y = y + LINE_SPACING;

        // Should aggregate.
        if (!(data_line_idx in numVisibleRequestsPerLine)) { numVisibleRequestsPerLine[data_line_idx] = 0; }
        numVisibleRequestsPerLine[data_line_idx] += numVisibleRequestsCurrLine;

        if (!(data_line_idx in numFailedRequestsPerLine)) { numFailedRequestsPerLine[data_line_idx] = 0; }
        numFailedRequestsPerLine[data_line_idx] += numFailedRequestsCurrLine;

        if (!(data_line_idx in totalSecondsPerLine)) { totalSecondsPerLine[data_line_idx] = 0; }
        totalSecondsPerLine[data_line_idx] += totalSecsCurrLine;

        title_end_idx = j;

        if (y > height) {
          // Make sure we don't miss the entry count of the rows beyond the viewport
          if (visual_line_offset_within_data_line == 0) {
            break;
          }
        }
      }

      {
        let nbreaks = this.TotalVisualHeight();
        // Draw a scroll bar on the left
        if (!(title_start_idx == 0 && title_end_idx == nbreaks - 1)) {

          const y0 = title_start_idx * height / nbreaks;
          const y1 = (1 + title_end_idx) * height / nbreaks;

          let highlighted = false;
          const THRESH = 8;
          if (this.MouseState.IsDraggingScrollBar()) {
            highlighted = true;
          }
          this.ScrollBarState.highlighted = highlighted;

          // If not dragging, let title_start_idx drive y0 and y1, else let the
          // user's input drive y0 and y1 and title_start_idx
          if (!this.MouseState.IsDraggingScrollBar()) {
            this.ScrollBarState.y0 = y0;
            this.ScrollBarState.y1 = y1;
          }

          if (highlighted) {
            ctx.fillStyle = "#FF3";
          } else {
            ctx.fillStyle = this.AccentColor;
          }
          ctx.fillRect(0, y0, SCROLL_BAR_WIDTH, y1 - y0);

        } else {
          this.ScrollBarState.y0 = undefined;
          this.ScrollBarState.y1 = undefined;
          this.ScrollBarState.highlighted = false;
        }
      }

      // Draw highlighted sections for the histograms
      if (this.IsTimeDistributionEnabled) {
        y = YBEGIN;
        for (let j = this.TitleStartIdx; j < this.Intervals.length; j++) {
          if (this.IpmiVizHistHighlighted[this.Titles[data_line_idx].title] != undefined) {
            let entry = HistogramThresholds[this.Titles[data_line_idx].title];
            const theSet =
                Array.from(this.IpmiVizHistHighlighted[this.Titles[data_line_idx].title]);
            for (let i = 0; i < theSet.length; i++) {
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
              const dx = HISTOGRAM_X - HISTOGRAM_W / 2 + HISTOGRAM_W * bidx;

              const r = HISTOGRAM_H * 0.17;
              ctx.beginPath();
              ctx.ellipse(dx, y, r, r, 0, 0, 3.14159 * 2);
              ctx.fill();
            }
          }
          y += LINE_SPACING;
        }
      }

      // Render number of visible requests versus totals
      ctx.textAlign = 'left';
      ctx.textBaseline = 'top';
      let totalOccs = 0, totalSecs = 0;
      if (this.IsHighlighted()) {
        ctx.fillStyle = '#00F';
        ctx.fillText('# / time', 3, TEXT_Y0);
        ctx.fillText('in selection', 3, TEXT_Y0 + LINE_SPACING - 2);
      } else {
        ctx.fillStyle = '#000';
        ctx.fillText('# / time', 3, TEXT_Y0);
        ctx.fillText('in viewport', 3, TEXT_Y0 + LINE_SPACING - 2);
      }

      let timeDesc = '';
      ctx.textBaseline = 'middle';
      last_data_line_idx = -999;

      for (let j = this.VisualLineStartIdx, i = 0;
               j < tvh && (YBEGIN + i*LINE_SPACING)<height; j++, i++) {
        const x = this.VisualLineIndexToDataLineIndex(j);
        if (x == undefined) break;
        const data_line_idx = x[0];
        if (data_line_idx == undefined) break;
        if (data_line_idx != last_data_line_idx) {
          let y1 = YBEGIN + LINE_SPACING * (i);
          let totalSeconds = totalSecondsPerLine[data_line_idx];
          if (totalSeconds < 1) {
            timeDesc = (totalSeconds * 1000.0).toFixed(toFixedPrecision) + 'ms';
          } else if (totalSeconds != undefined) {
            timeDesc = totalSeconds.toFixed(toFixedPrecision) + 's';
          } else {
            timeDesc = "???"
          }

          const n0 = numVisibleRequestsPerLine[data_line_idx];
          const n1 = numFailedRequestsPerLine[data_line_idx];
          let txt = '';
          if (n1 > 0) {
            txt = '' + n0 + '+' + n1 + ' / ' + timeDesc;
          } else {
            txt = '' + n0 + ' / ' + timeDesc;
          }

          const tw = ctx.measureText(txt).width;
          const PAD = 8;

          ctx.fillStyle = '#000';
          ctx.fillText(txt, 3, y1);
          totalOccs += numVisibleRequestsPerLine[data_line_idx];
          totalSecs += totalSeconds;
        }
        last_data_line_idx = data_line_idx;
      }

      // This does not get displayed correctly, so disabling for now
      //timeDesc = '';
      //if (totalSecs < 1) {
      //  timeDesc = '' + (totalSecs * 1000).toFixed(toFixedPrecision) + 'ms';
      //} else {
      //  timeDesc = '' + totalSecs.toFixed(toFixedPrecision) + 's';
      //}

      //ctx.fillText('Sum:', 3, y + 2 * LINE_SPACING);
      //ctx.fillText('' + totalOccs + ' / ' + timeDesc, 3, y + 3 * LINE_SPACING);

      // Update highlighted requests
      if (this.IsHighlightDirty) {
        this.HighlightedRequests = currHighlightedReqs;
        this.IsHighlightDirty = false;

        // Todo: This callback will be different for the DBus pane
        OnHighlightedChanged(HighlightedRequests);
      }

      // Render highlight statistics
      if (this.IsHighlighted()) {
        ctx.fillStyle = 'rgba(128,128,255,0.5)';
        let x0 = MapXCoord(
            t0, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
            this.UpperBoundTime);
        let x1 = MapXCoord(
            t1, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
            this.UpperBoundTime);
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
      // Only draw when the mouse is not over any hotizontal scroll bar
      let should_hide_cursor = false;

      if (this.MouseState.hoveredSide == "top_horizontal_scrollbar" ||
          this.MouseState.hoveredSide == "bottom_horizontal_scrollbar") {
        should_hide_cursor = true;
      }
      this.linked_views.forEach((v) => {
        if (v.MouseState.hoveredSide == "top_horizontal_scrollbar" ||
            v.MouseState.hoveredSide == "bottom_horizontal_scrollbar") {
          should_hide_cursor = true;
        }
      })

      if (this.MouseState.hovered == true &&
          this.MouseState.hoveredSide == undefined &&
          should_hide_cursor == false) {
        ctx.beginPath();
        ctx.strokeStyle = '#0000FF';
        ctx.lineWidth = 1;
        if (this.IsHighlighted()) {
          ctx.moveTo(this.MouseState.x, 0);
          ctx.lineTo(this.MouseState.x, height);
        } else {
          ctx.moveTo(this.MouseState.x, LINE_SPACING * 2);
          ctx.lineTo(this.MouseState.x, height - LINE_SPACING * 2);
        }
        ctx.stroke();

        if (this.IsHighlighted() == false) {
          let dispWidth = this.MouseState.x - LEFT_MARGIN;
          let label = '' +
              this.MouseXToTimestamp(this.MouseState.x)
                  .toFixed(toFixedPrecision) +
              's';
          let width0 = ctx.measureText(label).width;
          ctx.fillStyle = '#0000FF';
          ctx.textBaseline = 'bottom';
          ctx.textAlign = 'center';
          ctx.fillText(label, this.MouseState.x, height - LINE_SPACING);
          ctx.textBaseline = 'top';
          ctx.fillText(label, this.MouseState.x, LINE_SPACING + TEXT_Y0);
        }
      }

      // Tooltip box next to hovered entry
      if (theHoveredReq !== undefined) {
        this.RenderToolTip(
            ctx, theHoveredReq, theHoveredInterval, toFixedPrecision, height);
      }
    }  // End IsCanvasDirty
  }

  // Returns list of highlighted messages.
  // Format: [ Title, [Message] ]
  HighlightedMessages() {
    let ret = [];
    if (this.HighlightedRegion.t0 == -999 || this.HighlightedRegion.t1 == -999) { return ret; }
    const lb = Math.min(this.HighlightedRegion.t0, this.HighlightedRegion.t1);
    const ub = Math.max(this.HighlightedRegion.t0, this.HighlightedRegion.t1);
    for (let i=0; i<this.Titles.length; i++) {
      const title = this.Titles[i];
      if (title.header == true) continue;  // Do not include headers. TODO: Allow rectangular selection

      const line = [ title.title, [] ];
      const interval_idx = title.intervals_idxes[0];
      const intervals_i = this.Intervals[interval_idx];
      for (let j=0; j<intervals_i.length; j++) {
        const m = intervals_i[j];
        if (!(m[0] > ub || m[1] < lb)) {
          line[1].push(m);
        }
      }
      if (line[1].length > 0) {
        ret.push(line);
      }
    }
    return ret;
  }
};

// The extended classes have their own way of drawing popups for hovered entries
class IPMITimelineView extends TimelineView {
  RenderToolTip(
      ctx, theHoveredReq, theHoveredInterval, toFixedPrecision, height) {
    if (theHoveredReq == undefined) {
      return;
    }
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

    if (theHoveredReq.offset != undefined) {
      labels.push('Offset      : ' + theHoveredReq.offset);
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
    let dy = this.MouseState.y + DELTA_Y;
    if (dy + h > height) {
      dy = height - h;
    }
    let dx = this.MouseState.x;
    if (RIGHT_MARGIN - dx < w) dx -= (w + 2 * PAD);

    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.fillRect(dx, dy, w + 2 * PAD, h);

    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = '#FFFFFF';
    for (let i = 0; i < labels.length; i++) {
      ctx.fillText(
          labels[i], dx + PAD, dy + PAD + i * LINE_SPACING + LINE_SPACING / 2);
    }
  }
};

class DBusTimelineView extends TimelineView {
  RenderToolTip(
      ctx, theHoveredReq, theHoveredInterval, toFixedPrecision, height) {
    if (theHoveredReq == undefined) {
      return;
    }
    const PAD = 2, DELTA_Y = 14;

    let labels = [];
    let msg_type = theHoveredReq[0];
    let serial = theHoveredReq[2];
    let sender = theHoveredReq[3];
    let destination = theHoveredReq[4];
    let path = theHoveredReq[5];
    let iface = theHoveredReq[6];
    let member = theHoveredReq[7];

    let t0 = theHoveredInterval[0];
    let t1 = theHoveredInterval[1];

    labels.push('Message type: ' + msg_type);
    labels.push('Serial      : ' + serial);
    labels.push('Sender      : ' + sender);
    labels.push('Destination : ' + destination);
    labels.push('Path        : ' + path);
    labels.push('Interface   : ' + iface);
    labels.push('Member      : ' + member);

    let w = 1, h = LINE_SPACING * labels.length + 2 * PAD;
    for (let i = 0; i < labels.length; i++) {
      w = Math.max(w, ctx.measureText(labels[i]).width);
    }
    let dy = this.MouseState.y + DELTA_Y;
    if (dy + h > height) {
      dy = height - h;
    }
    let dx = this.MouseState.x;
    if (RIGHT_MARGIN - dx < w) dx -= (w + 2 * PAD);

    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.fillRect(dx, dy, w + 2 * PAD, h);

    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = '#FFFFFF';
    for (let i = 0; i < labels.length; i++) {
      ctx.fillText(
          labels[i], dx + PAD, dy + PAD + i * LINE_SPACING + LINE_SPACING / 2);
    }
  }
};

class BoostASIOHandlerTimelineView extends TimelineView {
  RenderToolTip(
      ctx, theHoveredReq, theHoveredInterval, toFixedPrecision, height) {
    if (theHoveredReq == undefined) {
      return;
    }
    const PAD = 2, DELTA_Y = 14;

    let labels = [];
    let create_time = theHoveredReq[2];
    let enter_time = theHoveredReq[3];
    let exit_time = theHoveredReq[4];
    let desc = theHoveredReq[5];

    let t0 = theHoveredInterval[0];
    let t1 = theHoveredInterval[1];

    labels.push('Creation time: ' + create_time);
    labels.push('Entry time   : ' + enter_time);
    labels.push('Exit time    : ' + exit_time);
    labels.push('Creation->Entry : ' + (enter_time - create_time));
    labels.push('Entry->Exit     : ' + (exit_time - enter_time));
    labels.push('Description  : ' + desc);

    let w = 1, h = LINE_SPACING * labels.length + 2 * PAD;
    for (let i = 0; i < labels.length; i++) {
      w = Math.max(w, ctx.measureText(labels[i]).width);
    }
    let dy = this.MouseState.y + DELTA_Y;
    if (dy + h > height) {
      dy = height - h;
    }
    let dx = this.MouseState.x;
    if (RIGHT_MARGIN - dx < w) dx -= (w + 2 * PAD);

    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.fillRect(dx, dy, w + 2 * PAD, h);

    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = '#FFFFFF';
    for (let i = 0; i < labels.length; i++) {
      ctx.fillText(
          labels[i], dx + PAD, dy + PAD + i * LINE_SPACING + LINE_SPACING / 2);
    }
  }
}
