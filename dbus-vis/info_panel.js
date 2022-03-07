// This function accomplishes drag-and-drop gestures by attaching temporary onmouse{up,move} events to the document
// Only the header is draggable, the content is not
function DragElement(elt) {
  var x1=0, y1=0, x2=0, y2=0;
  let header = document.getElementById(elt.id + "_header");
  if (header == undefined) {
    return;
  }
  header.onmousedown = DragMouseDown;

  function DragMouseDown(e) {
    e = e || window.event;
    e.preventDefault();
    // get the mouse cursor position at startup:
    x2 = e.clientX;
    y2 = e.clientY;
    document.onmouseup = MouseUp;
    // call a function whenever the cursor moves:
    document.onmousemove = MouseMove;
  }

  function MouseMove(e) {
    e = e || window.event;
    e.preventDefault();
    x1 = x2 - e.clientX; y1 = y2 - e.clientY;
    x2 = e.clientX; y2 = e.clientY;
    elt.style.top = (elt.offsetTop - y1) + "px";
    elt.style.left= (elt.offsetLeft- x1) + "px";
  }

  function MouseUp() {
    document.onmouseup = null;
    document.onmousemove = null;
  }
}

function GetTotalCount(titles_and_intervals) {
  let ret = 0;
  titles_and_intervals.forEach((ti) => {
    ret += ti[1].length;
  });
  return ret;
}

var g_info_panel_table = undefined;

// 6 rows
// Serial, Sender, Dest, Path, Iface, Member
function AddOneRowToTable(t, content) {
  let tr = document.createElement("tr");
  let td = document.createElement("td");
  td.colSpan = 6;
  t.appendChild(tr);
  tr.appendChild(td);
  td.innerText = content;
}

function AddDBusRowHeaderToTable(t) {
  const headers = [ "Serial", "Sender", "Destination", "Path", "Iface", "Member" ];
  let tr = document.createElement("tr");
  headers.forEach((h) => {
    let td = document.createElement("td");
    td.innerText = h;
    td.style.backgroundColor = "#888";
    tr.appendChild(td);
  });
  t.appendChild(tr);
}

function AddDBusMessageToTable(t, m) {
  const minfo = m[2];
  const cols = [
    minfo[2], // serial
    minfo[3], // sender
    minfo[4], // destination
    minfo[5], // path
    minfo[6], // iface
    minfo[7], // member
  ];
  let tr = document.createElement("tr");
  cols.forEach((c) => {
    let td = document.createElement("td");
    td.innerText = c;
    td.onclick = () => {
      console.log(m);
    };
    tr.appendChild(td);
  });
  t.appendChild(tr);
}

function UpdateHighlightedMessagesInfoPanel() {
  const MAX_ENTRIES_SHOWN = 10000; // Show a maximum of this many
  const ti_dbus = dbus_timeline_view.HighlightedMessages();
  const ti_ipmi = ipmi_timeline_view.HighlightedMessages();

  if (g_info_panel_table != undefined) {
    g_info_panel_table.remove(); // Remove from DOM tree
  }
  
  g_info_panel_table = document.createElement("table");

  const p = document.getElementById("highlighted_messages");
  p.style.display = "block";

  const x = document.getElementById("highlighted_messages_content");
  x.appendChild(g_info_panel_table);
  
  let nshown = 0;
  const count_dbus = GetTotalCount(ti_dbus);
  if (count_dbus > 0) {
    AddOneRowToTable(g_info_panel_table, count_dbus + " DBus messages");
    AddDBusRowHeaderToTable(g_info_panel_table);
    for (let i=0; i<ti_dbus.length; i++) {
      const title_and_msgs = ti_dbus[i];
      // Title
      AddOneRowToTable(g_info_panel_table, title_and_msgs[0]);
      for (let j=0; j<title_and_msgs[1].length; j++) {
        nshown ++;
        if (nshown > MAX_ENTRIES_SHOWN) break;
        AddDBusMessageToTable(g_info_panel_table, title_and_msgs[1][j]);
        // Messages
      }
    }
  }
  const count_ipmi = GetTotalCount(ti_ipmi);
  if (count_ipmi > 0) {
    AddOneRowToTable(g_info_panel_table, count_ipmi + " IPMI messages");
  }
  if (nshown > MAX_ENTRIES_SHOWN) {
    AddOneRowToTable(g_info_panel_table, "Showing only " + MAX_ENTRIES_SHOWN + " entries");
  }
}