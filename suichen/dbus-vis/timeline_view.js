// Default range: 0 to 300s, shared between both views
var RANGE_LEFT_INIT = 0;
var RANGE_RIGHT_INIT = 300;

// Global timeline start
var g_StartingSec = undefined;



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
    this.Titles = [];
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
    this.TitleStartIdx = 0;

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
      hoveredLineIndex: -999,
      hoveredSide: undefined
    };
    this.HighlightedRegion = {t0: -999, t1: -999};

    // The linked view will move and zoom with this view
    this.linked_views = [];
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
    // Update highlighted area
    if (ipmi_timeline_view.MouseState.pressed == true) {
      ipmi_timeline_view.HighlightedRegion.t1 =
          ipmi_timeline_view.MouseXToTimestamp(ipmi_timeline_view.MouseState.x);
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
    this.MouseState.hoveredSide = undefined;
    this.MouseState.hoveredLineIndex = -999;
    if (lineIndex < this.Intervals.length) {
      this.MouseState.hoveredLineIndex = lineIndex;
      if (this.MouseState.x <= PAD + LINE_SPACING / 2 + LEFT_MARGIN &&
          this.MouseState.x >= PAD + LEFT_MARGIN) {
        this.MouseState.hoveredSide = 'left';
        this.IsCanvasDirty = true;
      } else if (
          this.MouseState.x <= RIGHT_MARGIN - PAD &&
          this.MouseState.x >= RIGHT_MARGIN - PAD - LINE_SPACING / 2) {
        this.MouseState.hoveredSide = 'right';
        this.IsCanvasDirty = true;
      }
    }
  }

  OnMouseLeave() {
    this.MouseState.hovered = false;
    this.IsCanvasDirty = true;
  }

  // Assume event.button is zero (left mouse button)
  OnMouseDown(iter = 1) {
    // If hovering over an overflowing triangle, warp to the nearest overflowed
    //     request on that line
    if (this.MouseState.hoveredLineIndex >= 0 &&
        this.MouseState.hoveredLineIndex < this.Intervals.length &&
        this.MouseState.hoveredSide != undefined) {
      let line = this.Intervals[this.MouseState.hoveredLineIndex];
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
    if (tx >= t0 && tx <= t1) {
      // If clicking inside highlighted area, zoom around the area
      this.BeginSetBoundaryAnimation(t0, t1);
      this.Unhighlight();
      this.IsCanvasDirty = true;

      this.linked_views.forEach(function(v) {
        v.BeginSetBoundaryAnimation(t0, t1, 0);
        v.Unhighlight();
        v.IsCanvasDirty = false;
      });
    } else {  // Otherwise start a new dragging session
      this.MouseState.pressed = true;
      this.HighlightedRegion.t0 = this.MouseXToTimestamp(this.MouseState.x);
      this.HighlightedRegion.t1 = this.HighlightedRegion.t0;
      this.IsCanvasDirty = true;
    }
  }

  // Assume event.button == 0 (left mouse button)
  OnMouseUp() {
    this.MouseState.pressed = false;
    this.IsCanvasDirty = true;
    this.UnhighlightIfEmpty();
    this.IsHighlightDirty = true;
  }

  UnhighlightIfEmpty() {
    if (this.HighlightedRegion.t0 == this.HighlightedRegion.t1) {
      this.Unhighlight();
      this.IsCanvasDirty = true;
      return true;
    } else
      return false;
  }

  ScrollY(delta) {
    this.TitleStartIdx += delta;
    if (this.TitleStartIdx < 0) {
      this.TitleStartIdx = 0;
    } else if (this.TitleStartIdx >= this.Titles.length) {
      this.TitleStartIdx = this.Titles.length - 1;
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
      if (this.MouseState.hoveredLineIndex >= 0 &&
          this.MouseState.hoveredLineIndex < this.Intervals.length) {
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
        86400, 10800, 3600,   1800, 1200, 600, 300, 120, 
        60,     30,     10,      5,       2,
        1,     0.5,   0.2,    0.1,    0.05,   0.02,    0.01,    0.005,
        0.002, 0.001, 0.0005, 0.0002, 0.0001, 0.00005, 0.00002, 0.00001
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
      let numVisibleRequestsPerLine = [];
      let numFailedRequestsPerLine = [];
      let totalSecondsPerLine = [];

      // Range of Titles that were displayed
      let title_start_idx = this.TitleStartIdx, title_end_idx = title_start_idx;

      for (let j = this.TitleStartIdx; j < this.Intervals.length; j++) {
        ctx.textBaseline = 'middle';
        ctx.textAlign = 'right';
        let desc_width = 0;
        if (NetFnCmdToDescription[this.Titles[j]] != undefined) {
          let desc = ' (' + NetFnCmdToDescription[this.Titles[j]] + ')';
          desc_width = ctx.measureText(desc).width;
          ctx.fillStyle = '#888';  // Grey
          ctx.fillText(desc, LEFT_MARGIN - 3, y);
        }

        // Plot histogram
        if (this.IsTimeDistributionEnabled == true) {
          const t = this.Titles[j];
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
        ctx.textAlignment = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillStyle = '#000000';  // Revert to Black
        ctx.strokeStyle = '#000000';
        let tj_draw = this.Titles[j];
        const title_disp_length_limit = this.GetTitleWidthLimit();
        if (tj_draw != undefined && tj_draw.length > title_disp_length_limit) {
          tj_draw = tj_draw.substr(0, title_disp_length_limit) + '...'
        }
        ctx.fillText(tj_draw, LEFT_MARGIN - 3 - desc_width, y);

        let numOverflowEntriesToTheLeft = 0;  // #entries below the lower bound
        let numOverflowEntriesToTheRight =
            0;                               // #entries beyond the upper bound
        let numVisibleRequestsCurrLine = 0;  // #entries visible
        let totalSecsCurrLine = 0;           // Total duration in seconds
        let numFailedRequestsCurrLine = 0;

        const intervals_j = this.Intervals[j];
        if (intervals_j != undefined) {
          for (let i = 0; i < intervals_j.length; i++) {
            let lb = intervals_j[i][0], ub = intervals_j[i][1];
            if (lb > ub)
              continue;  // Unmatched (only enter & no exit timestamp)

            let isHighlighted = false;
            let durationUsec =
                (intervals_j[i][1] - intervals_j[i][0]) * 1000000;
            let lbub = [lb, ub];
            if (this.IsHighlighted()) {
              if (IsIntersected(lbub, highlightedInterval)) {
                numIntersected++;
                isHighlighted = true;
                currHighlightedReqs.push(intervals_j[i][2]);
              }
            }

            if (ub < this.LowerBoundTime) {
              numOverflowEntriesToTheLeft++;
              continue;
            }
            if (lb > this.UpperBoundTime) {
              numOverflowEntriesToTheRight++;
              continue;
            }
            // Failed request
            if (ub == undefined && lb < this.UpperBoundTime) {
              numOverflowEntriesToTheLeft++;
              continue;
            }

            let dx0 = MapXCoord(
                    lb, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
                    this.UpperBoundTime),
                dx1 = MapXCoord(
                    ub, LEFT_MARGIN, RIGHT_MARGIN, this.LowerBoundTime,
                    this.UpperBoundTime),
                dy0 = y - LINE_HEIGHT / 2, dy1 = y + LINE_HEIGHT / 2;

            dx0 = Math.max(dx0, LEFT_MARGIN);
            dx1 = Math.min(dx1, RIGHT_MARGIN);
            let dw = Math.max(0, dx1 - dx0);

            if (isHighlighted) {
              ctx.fillStyle = 'rgba(128,128,255,0.5)';
              ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
            }

            let isCurrentReqHovered = false;
            // Intersect with mouse using pixel coordinates
            if (IsIntersectedPixelCoords(
                    [dx0, dx0 + dw], [this.MouseState.x, this.MouseState.x]) &&
                IsIntersectedPixelCoords(
                    [dy0, dy1], [this.MouseState.y, this.MouseState.y])) {
              ctx.fillStyle = 'rgba(255,255,0,0.5)';
              ctx.fillRect(dx0, dy0, dw, dy1 - dy0);
              theHoveredReq = this.Intervals[j][i][2];
              theHoveredInterval = this.Intervals[j][i];
              isCurrentReqHovered = true;
            }

            ctx.lineWidth = 0.5;

            
            // If this request is taking too long/is quick enough, use red/green
            let entry = HistogramThresholds[this.Titles[j]];
            
            let isError = false;
            if (intervals_j[i][3] == "error") { isError = true; }
            
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

            const duration = this.Intervals[j][i][1] - this.Intervals[j][i][0];
            if (!isNaN(duration)) {
                if (isError) {
                  ctx.fillStyle = "rgba(192, 128, 128, 0.8)";
                  ctx.fillRect(dx0, dy0, dw, dy1-dy0);
                  ctx.strokeStyle = "rgba(192, 128, 128, 1)";
                }
                
                ctx.strokeRect(dx0, dy0, dw, dy1 - dy0);
                numVisibleRequests++;
              } else {
                // This entry has only a beginning and not an end
                // perhaps the original method call did not return
                if (isCurrentReqHovered) { ctx.fillStyle = "rgba(192, 192, 0, 0.8)"; }
                else { ctx.fillStyle = "rgba(255, 128, 128, 0.8)"; }
                ctx.beginPath();
                ctx.arc(dx0, (dy0+dy1)/2, HISTOGRAM_H*0.17, 0, 2*Math.PI);
                ctx.fill();
              }

            
            // Affects whether this req will be reflected in the aggregate info
            //     section
            if ((isAggregateSelection == false) ||
                (isAggregateSelection == true && isHighlighted == true)) {
            
            
              if (!isNaN(duration)) {
                numVisibleRequestsCurrLine++;
                totalSecsCurrLine += duration;
              } else {
                numFailedRequestsCurrLine++;
              }
              
              // If a histogram exists for Titles[j], process the highlighted
              //     histogram buckets
              if (GetHistoryHistogram()[this.Titles[j]] != undefined) {
                let histogramEntry = GetHistoryHistogram()[this.Titles[j]];
                let bucketInterval = (histogramEntry[1] - histogramEntry[0]) /
                    histogramEntry[2].length;
                let bucketIndex =
                    Math.floor(
                        (durationUsec - histogramEntry[0]) / bucketInterval) /
                    histogramEntry[2].length;

                if (this.IpmiVizHistHighlighted[this.Titles[j]] == undefined) {
                  this.IpmiVizHistHighlighted[this.Titles[j]] = new Set();
                }
                let entry = this.IpmiVizHistHighlighted[this.Titles[j]];
                entry.add(bucketIndex);
              }
            }
          }
        }

        // Triangle markers for entries outside of the viewport
        {
          const PAD = 2, H = LINE_SPACING;
          if (this.MouseState.hoveredLineIndex == j &&
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

          if (this.MouseState.hoveredLineIndex == j &&
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
        y = y + LINE_SPACING;
        numVisibleRequestsPerLine.push(numVisibleRequestsCurrLine);
        numFailedRequestsPerLine.push(numFailedRequestsCurrLine);
        totalSecondsPerLine.push(totalSecsCurrLine);

        title_end_idx = j;
        if (y > height) break;
      }

      // Draw a scroll bar on the left
      if (!(title_start_idx == 0 && title_end_idx == this.Titles.length - 1)) {
        let nbreaks = this.Titles.length;
        let y0 = title_start_idx * height / nbreaks;
        let y1 = (1 + title_end_idx) * height / nbreaks;
        ctx.fillStyle = this.AccentColor;
        ctx.fillRect(0, y0, 4, y1 - y0);
      }

      // Draw highlighted sections for the histograms
      if (this.IsTimeDistributionEnabled) {
        y = YBEGIN;
        for (let j = this.TitleStartIdx; j < this.Intervals.length; j++) {
          if (this.IpmiVizHistHighlighted[this.Titles[j]] != undefined) {
            let entry = HistogramThresholds[this.Titles[j]];
            const theSet =
                Array.from(this.IpmiVizHistHighlighted[this.Titles[j]]);
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
        ctx.fillText("in selection", 3, TEXT_Y0 + LINE_SPACING - 2);
      } else {
        ctx.fillStyle = '#000';
        ctx.fillText('# / time', 3, TEXT_Y0);
        ctx.fillText("in viewport", 3, TEXT_Y0 + LINE_SPACING - 2);
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
        
        const n0 = numVisibleRequestsPerLine[i];
        const n1 = numFailedRequestsPerLine[i];
        let txt = "";
        if (n1 > 0) {
          txt = '' + n0 + '+' + n1 + ' / ' + timeDesc;
        } else {
          txt = '' + n0 + " / " + timeDesc;
        }
        ctx.fillText(txt, 3, y);
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
      if (this.MouseState.hovered == true &&
          this.MouseState.hoveredSide == undefined) {
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
