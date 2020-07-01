// This file deals with preprocessing the parsed DBus timeline data.
// Data and Timestamps are separate b/c dbus-pcap does not include
// timestamps in JSON output so we need to export both formats
// (JSON and text)
var Data_DBus = [];
var Timestamps_DBus = [];

// Main view object
var dbus_timeline_view = new DBusTimelineView();
var sensors_timeline_view = new DBusTimelineView(); // Same DBusTimelineView type, just that it will have only sensor propertieschanged events

// group-by condition changes
{
  const tags = [
    'dbus_column1', 'dbus_column2', 'dbus_column3', 'dbus_column4',
    'dbus_column5', 'dbus_column6', 'dbus_column7'
  ];
  for (let i = 0; i < 7; i++) {
    document.getElementById(tags[i]).addEventListener(
        'click', OnGroupByConditionChanged_DBus);
  }
}

// Called from renderer.Render()
function draw_timeline_sensors(ctx) {
  sensors_timeline_view.Render(ctx);
}

// Called from renderer.Render()
function draw_timeline_dbus(ctx) {
  dbus_timeline_view.Render(ctx);
}

let Canvas_DBus = document.getElementById('my_canvas_dbus');

const IDXES = {
  'Type': 0,
  'Timestamp': 1,
  'Serial': 2,
  'Sender': 3,
  'Destination': 4,
  'Path': 5,
  'Interface': 6,
  'Member': 7
};

// This "group" is based on the content of the DBus
// It is independent of the "group_by" of the meta-data (sender/destination/
// path/interface/member) of a DBus message
//
// Input is processed message and some basic statistics needed for categorizing
//
const DBusMessageContentKey = function(msg, cxn_occ) {
  let ret = undefined;
  const type = msg[IDXES["Type"]];
  const dest = msg[IDXES["Destination"]];
  const path = msg[IDXES["Path"]];
  const iface = msg[IDXES["Interface"]];
  const member = msg[IDXES["Member"]];
  const sender = msg[IDXES["Sender"]];

  if (sender == "s" || sender == "sss") {
    console.log(msg)
  }
  
  if (type == "sig") {
    if (path.indexOf("/xyz/openbmc_project/sensors/") != -1 &&
        iface == "org.freedesktop.DBus.Properties" &&
        member == "PropertiesChanged") {
      ret = "Sensor PropertiesChanged Signals";
    }
  } else if (type == "mc") {
    if (dest == "xyz.openbmc_project.Ipmi.Host" &&
        path == "/xyz/openbmc_project/Ipmi" &&
        iface == "xyz.openbmc_project.Ipmi.Server" &&
        member == "execute") {
      ret = "IPMI Daemon";
    }
  }
  
  if (ret == undefined && cxn_occ[sender] <= 10) {
    ret = "Total 10 messages or less"
  }

  if (ret == undefined && type == "mc") {
    if (path.indexOf("/xyz/openbmc_project/sensors/") == 0 &&
    iface == "org.freedesktop.DBus.Properties" &&
    (member.startsWith("Get") || member.startsWith("Set"))) {
      ret = "Sensor Get/Set";
    }
  }

  if (ret == undefined) {
    ret = "Uncategorized";
  }

  return ret;
}

function Group_DBus(preprocessed, group_by) {
  let grouped = {};  // [content key][sort key] -> packet

  let cxn_occ = {}; // How many times have a specific service appeared?
  preprocessed.forEach((pp) => {
    const cxn = pp[IDXES["Sender"]];
    if (cxn_occ[cxn] == undefined) {
      cxn_occ[cxn] = 0;
    }
    cxn_occ[cxn]++;
  });

  for (var n = 0; n < preprocessed.length; n++) {
    var key = '';
    for (var i = 0; i < group_by.length; i++) {
      if (i > 0) key += ' ';
      key += ('' + preprocessed[n][IDXES[group_by[i]]]);
    }

    // "Content Key" is displayed on the "Column Headers"
    const content_group = DBusMessageContentKey(preprocessed[n], cxn_occ);

    // Initialize the "Collapsed" array here
    // TODO: this should ideally not be specific to the dbus_interface_view instance
    if (dbus_timeline_view.HeaderCollapsed[content_group] == undefined) {
      dbus_timeline_view.HeaderCollapsed[content_group] = false;  // Not collapsed by default
    }

    if (grouped[content_group] == undefined) {
      grouped[content_group] = [];
    }
    let grouped1 = grouped[content_group];

    if (grouped1[key] == undefined) grouped1[key] = [];
    grouped1[key].push(preprocessed[n]);
  }
  return grouped;
}

function OnGroupByConditionChanged_DBus() {
  var tags = [
    'dbus_column1', 'dbus_column2', 'dbus_column3', 'dbus_column4',
    'dbus_column5', 'dbus_column6', 'dbus_column7'
  ];
  const v = dbus_timeline_view;
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
  let preproc = Preprocess_DBusPcap(
      Data_DBus, Timestamps_DBus);  // should be from dbus_pcap
  let grouped = Group_DBus(preproc, v.GroupBy);
  GenerateTimeLine_DBus(grouped);
  dbus_timeline_view.IsCanvasDirty = true;
}

// Todo: put g_StartingSec somewhere that's common between sensors and non-sensors
function GenerateTimeLine_DBus(grouped) {
  let intervals = [];
  let titles = [];
  g_StartingSec = undefined;

  // First, turn "content keys" into headers in the flattened layout
  const content_keys = Object.keys(grouped);

  const keys = Object.keys(grouped);
  let sortedKeys = keys.slice();

  let interval_idx = 0;  // The overall index into the intervals array

  for (let x=0; x<content_keys.length; x++) {
    const content_key = content_keys[x];
    // Per-content key
    const grouped1 = grouped[content_key];
    const keys1 = Object.keys(grouped1);

    let the_header = { "header":true, "title":content_key, "intervals_idxes":[] };
    titles.push(the_header);
    // TODO: this currently depends on the dbus_timeline_view instance
    const collapsed = dbus_timeline_view.HeaderCollapsed[content_key];

    for (let i = 0; i < keys1.length; i++) {
      // The Title array controls which lines are drawn. If we con't push the header
      // it will not be drawn (thus giving a "collapsed" visual effect.)
      if (!collapsed) {
        titles.push({ "header":false, "title":keys1[i], "intervals_idxes":[interval_idx] });
      }


      line = [];
      for (let j = 0; j < grouped1[keys1[i]].length; j++) {
        let entry = grouped1[keys1[i]][j];
        let t0 = parseFloat(entry[1]) / 1000.0;
        let t1 = parseFloat(entry[8]) / 1000.0;

        // Modify time shift delta if IPMI dataset is loaded first
        if (g_StartingSec == undefined) {
          g_StartingSec = t0;
        }
        g_StartingSec = Math.min(g_StartingSec, t0);
        const outcome = entry[9];
        line.push([t0, t1, entry, outcome, 0]);
      }

      the_header.intervals_idxes.push(interval_idx);  // Keep the indices into the intervals array for use in rendering
      intervals.push(line);
      interval_idx ++;
    }

    // Compute a set of "merged intervals" for each content_key
    let rise_fall_edges = [];
    the_header.intervals_idxes.forEach((i) => {
      intervals[i].forEach((t0t1) => {
        if (t0t1[0] <= t0t1[1]) {  // For errored-out method calls, the end time will be set to a value smaller than the start tiem
          rise_fall_edges.push([t0t1[0], 0]);  // 0 is a rising edge
          rise_fall_edges.push([t0t1[1], 1]);  // 1 is a falling edge
        }
      })
    });

    let merged_intervals = [], 
        current_interval = [undefined, undefined, 0];  // start, end, weight
    rise_fall_edges.sort();
    let i = 0, level = 0;
    while (i<rise_fall_edges.length) {
      let timestamp = rise_fall_edges[i][0];
      while (i < rise_fall_edges.length && timestamp == rise_fall_edges[i][0]) {
        switch (rise_fall_edges[i][1]) {
          case 0: {  // rising edge
            if (level == 0) {
              current_interval[0] = timestamp;
              current_interval[2] ++;
            }
            level ++;
            break;
          }
          case 1: {  // falling edge
            level --;
            if (level == 0) {
              current_interval[1] = timestamp;
              merged_intervals.push(current_interval);
              current_interval = [undefined, undefined, 0];
            }
            break;
          }
        }
        i++;
      }
    }
    the_header.merged_intervals = merged_intervals;
  }

  // Time shift
  for (let i = 0; i < intervals.length; i++) {
    for (let j = 0; j < intervals[i].length; j++) {
      let x = intervals[i][j];
      x[0] -= g_StartingSec;
      x[1] -= g_StartingSec;
    }
  }
  // merged intervals should be time-shifted as well
  titles.forEach((t) => {
    if (t.header == true) {
      t.merged_intervals.forEach((mi) => {
        mi[0] -= g_StartingSec;
        mi[1] -= g_StartingSec;
      })
    }
  })

  dbus_timeline_view.Intervals = intervals.slice();
  dbus_timeline_view.Titles    = titles.slice();
  dbus_timeline_view.LayoutForOverlappingIntervals();
}

Canvas_DBus.onmousemove =
    function(event) {
  const v = dbus_timeline_view;
  v.MouseState.x = event.pageX - this.offsetLeft;
  v.MouseState.y = event.pageY - this.offsetTop;
  if (v.MouseState.pressed == true &&
    v.MouseState.hoveredSide == 'timeline') {  // Update highlighted area
    v.HighlightedRegion.t1 = v.MouseXToTimestamp(v.MouseState.x);
  }
  v.OnMouseMove();
  v.IsCanvasDirty = true;

  v.linked_views.forEach(function(u) {
    u.MouseState.x = event.pageX - Canvas_DBus.offsetLeft;
    u.MouseState.y = undefined;                  // Do not highlight any entry or the horizontal scroll bars
    if (u.MouseState.pressed == true &&
      v.MouseState.hoveredSide == 'timeline') {  // Update highlighted area
      u.HighlightedRegion.t1 = u.MouseXToTimestamp(u.MouseState.x);
    }
    u.OnMouseMove();
    u.IsCanvasDirty = true;
  });
}

Canvas_DBus.onmousedown = function(event) {
  if (event.button == 0) {
    dbus_timeline_view.OnMouseDown();
  }
};

Canvas_DBus.onmouseup =
    function(event) {
  if (event.button == 0) {
    dbus_timeline_view.OnMouseUp();
  }
}

Canvas_DBus.onmouseleave = 
    function(event) {
  dbus_timeline_view.OnMouseLeave();
}

Canvas_DBus.onwheel = function(event) {
  dbus_timeline_view.OnMouseWheel(event);
}
