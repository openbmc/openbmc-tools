// This script deals with Boost ASIO handlers
// Will need to add code to visualize one-shot events

// Fields: HandlerNumber, Level,
// Creation time, Enter time, Exit time, EnterDescription,
//    [ [Operation, time] ]

// Will use g_StartingSec as the starting of epoch

var ASIO_Data = [];
var ASIO_Timestamp = [];

function FindFirstEntrySlot(slots) {
  let i = 0;
  for (; i < slots.length; i++) {
    if (slots[i] == undefined) break;
  }
  if (i >= slots.length) slots.push(undefined);
  return i;
}

function SimplifyDesc(desc) {
  const idx0 = desc.indexOf('0x');
  if (idx0 == -1)
    return desc;
  else {
    const d1 = desc.substr(idx0 + 2);
    let idx1 = 0;
    while (idx1 + 1 < d1.length &&
           ((d1[idx1] >= '0' && d1[idx1] <= '9') ||
            (d1[idx1] >= 'A' && d1[idx1] <= 'F') ||
            (d1[idx1] >= 'a' && d1[idx1] <= 'f'))) {
      idx1++;
    }
    return desc.substr(0, idx0) + d1.substr(idx1)
  }
}

function ParseBoostHandlerTimeline(content) {
  let parsed_entries = [];
  const lines = content.split('\n');
  let slots = [];               // In-flight handlers
  let in_flight_id2level = {};  // In-flight ID to level

  for (let lidx = 0; lidx < lines.length; lidx++) {
    const line = lines[lidx];
    if (line.startsWith('@asio|') == false) continue;
    const sp = line.split('|');

    const tag = sp[0], ts = sp[1], action = sp[2], desc = sp[3];
    let handler_id = -999;
    let ts_sec = parseFloat(ts);
    const simp_desc = SimplifyDesc(desc);

    if (action.indexOf('*') != -1) {
      const idx = action.indexOf('*');
      const handler_id = parseInt(action.substr(idx + 1));
      const level = FindFirstEntrySlot(slots);

      // Create an entry here
      let entry = [
        handler_id, level, ts_sec, undefined, undefined, desc, simp_desc, []
      ];

      slots[level] = entry;
      in_flight_id2level[handler_id] = level;
    } else if (action[0] == '>') {  // The program enters handler number X
      handler_id = parseInt(action.substr(1));
      if (handler_id in in_flight_id2level) {
        const level = in_flight_id2level[handler_id];
        let entry = slots[level];
        entry[3] = ts_sec;
      }
    } else if (action[0] == '<') {
      handler_id = parseInt(action.substr(1));
      if (handler_id in in_flight_id2level) {
        const level = in_flight_id2level[handler_id];
        let entry = slots[level];
        entry[4] = ts_sec;
        slots[level] = undefined;
        parsed_entries.push(entry);
        delete in_flight_id2level[handler_id];
      }
    } else if (action[0] == '.') {  // syscalls
    }
  }

  console.log(
      'Boost handler log: ' + parsed_entries.length + ' entries' +
      ', ' + slots.length + ' levels');
  ASIO_Data = parsed_entries;
  return parsed_entries;
}

function Group_ASIO(preprocessed, group_by) {
  let grouped = {};
  const IDXES = {'Layout Level': 1, 'Description': 5, 'Description1': 6};
  for (var n = 0; n < preprocessed.length; n++) {
    var key = ''
    for (var i = 0; i < group_by.length; i++) {
      if (i > 0) key += ' ';
      key += ('' + preprocessed[n][IDXES[group_by[i]]]);
    }
    if (grouped[key] == undefined) grouped[key] = [];
    grouped[key].push(preprocessed[n]);
  }
  return grouped;
}

function OnGroupByConditionChanged_ASIO() {
  var tags = ['bah1', 'bah2', 'bah3'];
  const v = boost_asio_handler_timeline_view;
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
  let preproc = ASIO_Data;
  let grouped = Group_ASIO(preproc, v.GroupBy);
  GenerateTimeLine_ASIO(grouped);
  boost_asio_handler_timeline_view.IsCanvasDirty = true;
}

function GenerateTimeLine_ASIO(grouped) {
  const keys = Object.keys(grouped);
  let sortedKeys = keys.slice();
  let intervals = [];
  let titles = [];

  const was_starting_time_undefined = (g_StartingSec == undefined);

  for (let i = 0; i < sortedKeys.length; i++) {
    titles.push({"header":false, "title":sortedKeys[i], "intervals_idxes":[i] });
    line = [];
    for (let j = 0; j < grouped[sortedKeys[i]].length; j++) {
      let entry = grouped[sortedKeys[i]][j];
      let t0 = parseFloat(entry[3]);
      let t1 = parseFloat(entry[4]);

      if (was_starting_time_undefined) {
        if (g_StartingSec == undefined) {
          g_StartingSec = t0;
        }
        g_StartingSec = Math.min(g_StartingSec, t0);
      }

      line.push([t0, t1, entry, 'ok', 0]);
    }
    intervals.push(line);
  }

  // Time shift
  for (let i = 0; i < intervals.length; i++) {
    for (let j = 0; j < intervals[i].length; j++) {
      let x = intervals[i][j];
      x[0] -= g_StartingSec;
      x[1] -= g_StartingSec;
    }
  }

  boost_asio_handler_timeline_view.Intervals = intervals.slice();
  boost_asio_handler_timeline_view.Titles = titles.slice();
  boost_asio_handler_timeline_view.LayoutForOverlappingIntervals();
}

// Main view object for Boost handler timeline

boost_asio_handler_timeline_view = new BoostASIOHandlerTimelineView();
boost_asio_handler_timeline_view.IsCanvasDirty = true;

function draw_timeline_boost_asio_handler(ctx) {
  boost_asio_handler_timeline_view.Render(ctx);
}

let Canvas_Asio = document.getElementById('my_canvas_boost_asio_handler');

Canvas_Asio.onmousemove = function(event) {
  const v = boost_asio_handler_timeline_view;
  v.MouseState.x = event.pageX - this.offsetLeft;
  v.MouseState.y = event.pageY - this.offsetTop;
  if (v.MouseState.pressed == true) {  // Update highlighted area
    v.HighlightedRegion.t1 = v.MouseXToTimestamp(v.MouseState.x);
  }
  v.OnMouseMove();
  v.IsCanvasDirty = true;

  v.linked_views.forEach(function(u) {
    u.MouseState.x = event.pageX - Canvas_Asio.offsetLeft;
    u.MouseState.y = 0;                  // Do not highlight any entry
    if (u.MouseState.pressed == true) {  // Update highlighted area
      u.HighlightedRegion.t1 = u.MouseXToTimestamp(u.MouseState.x);
    }
    u.OnMouseMove();
    u.IsCanvasDirty = true;
  });
};

Canvas_Asio.onmousedown = function(event) {
  if (event.button == 0) {
    boost_asio_handler_timeline_view.OnMouseDown();
  }
};

Canvas_Asio.onmouseup = function(event) {
  if (event.button == 0) {
    boost_asio_handler_timeline_view.OnMouseUp();
  }
};

Canvas_Asio.onwheel = function(event) {
  boost_asio_handler_timeline_view.OnMouseWheel(event);
}
