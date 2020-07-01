// This file parses ASCII text-encoded dbus message dump.

function extractUsec(line) {
  let i0 = line.indexOf('time=');
  if (i0 == -1) {
    return BigInt(-1);
  }
  let line1 = line.substr(i0);
  let i1 = line1.indexOf(' ');
  if (i1 == -1) {
    return BigInt(-1);
  }
  let line2 = line1.substr(5, i1 - 5);
  let sp = line2.split('.');
  return BigInt(sp[0]) * BigInt(1000000) + BigInt(sp[1]);
}

function extractInt(line, kw) {
  let N = kw.length;
  let i0 = line.indexOf(kw);
  if (i0 == -1) {
    return null;
  }
  let line1 = line.substr(i0);
  let i1 = line1.indexOf(' ');
  if (i1 == -1) {
    i1 = line.length;
  }
  let line2 = line1.substr(N, i1 - N);
  return parseInt(line2);
}

function extractSerial(line) {
  return extractInt(line, 'serial=');
}

function extractReplySerial(line) {
  return extractInt(line, 'reply_serial=');
}

// Returns [byte, i+1] if successful
// Returns [null, i  ] if unsuccessful
function munchByte(lines, i) {
  if (i >= lines.length) {
    return [null, i]
  }
  let l = lines[i];
  let idx = l.indexOf('byte');
  if (idx != -1) {
    return [parseInt(l.substr(idx + 4), 10), i + 1];
  } else {
    return [null, i];
  }
}

// array of bytes "@"
function munchArrayOfBytes1(lines, i) {
  let l = lines[i];
  let idx = l.indexOf('array of bytes "');
  if (idx != -1) {
    let the_ch = l.substr(idx + 16, 1);
    return [[the_ch.charCodeAt(0)], i + 1];
  } else {
    return [null, i];
  }
}

function munchArrayOfBytes2(lines, i) {
  let l = lines[i];
  let idx = l.indexOf('array of bytes [');
  if (idx == -1) {
    idx = l.indexOf('array [');
  }
  if (idx != -1) {
    let j = i + 1;
    let payload = [];
    while (true) {
      if (j >= lines.length) {
        break;
      }
      l = lines[j];
      let sp = l.trim().split(' ');
      let ok = true;
      for (let k = 0; k < sp.length; k++) {
        let b = parseInt(sp[k], 16);
        if (isNaN(b)) {
          ok = false;
          break;
        } else {
          payload.push(b);
        }
      }
      if (!ok) {
        j--;
        break;
      } else
        j++;
    }
    return [payload, j];
  } else {
    return [null, i];
  }
}

function munchArrayOfBytes(lines, i) {
  if (i >= lines.length) return [null, i];

  let x = munchArrayOfBytes1(lines, i);
  if (x[0] != null) {
    return x;
  }
  x = munchArrayOfBytes2(lines, i);
  if (x[0] != null) {
    return x;
  }
  return [null, i];
}

// ReceivedMessage
function munchLegacyMessageStart(lines, i) {
  let entry = {
    netfn: 0,
    lun: 0,
    cmd: 0,
    serial: 0,
    start_usec: 0,
    end_usec: 0,
    request: [],
    response: []
  };

  let ts = extractUsec(lines[i]);
  entry.start_usec = ts;

  let x = munchByte(lines, i + 1);
  if (x[0] == null) {
    return [null, i];
  }
  entry.serial = x[0];
  let j = x[1];

  x = munchByte(lines, j);
  if (x[0] == null) {
    return [null, i];
  }
  entry.netfn = x[0];
  j = x[1];

  x = munchByte(lines, j);
  if (x[0] == null) {
    return [null, i];
  }
  entry.lun = x[0];
  j = x[1];

  x = munchByte(lines, j);
  if (x[0] == null) {
    return [null, i];
  }
  entry.cmd = x[0];
  j = x[1];

  x = munchArrayOfBytes(lines, j);
  if (x[0] == null) {
    return [null, i];
  }
  entry.request = x[0];
  j = x[1];

  return [entry, j];
}

function munchLegacyMessageEnd(lines, i, in_flight, parsed_entries) {
  let ts = extractUsec(lines[i]);

  let x = munchByte(lines, i + 1);
  if (x[0] == null) {
    return [null, i];
  }  // serial
  let serial = x[0];
  let j = x[1];

  let entry = null;
  if (serial in in_flight) {
    entry = in_flight[serial];
    delete in_flight[serial];
  } else {
    return [null, i];
  }

  entry.end_usec = ts;

  x = munchByte(lines, j);  // netfn
  if (x[0] == null) {
    return [null, i];
  }
  if (entry.netfn + 1 == x[0]) {
  } else {
    return [null, i];
  }
  j = x[1];

  x = munchByte(lines, j);  // lun (not used right now)
  if (x[0] == null) {
    return [null, i];
  }
  j = x[1];

  x = munchByte(lines, j);  // cmd
  if (x[0] == null) {
    return [null, i];
  }
  if (entry.cmd == x[0]) {
  } else {
    return [null, i];
  }
  j = x[1];

  x = munchByte(lines, j);  // cc
  if (x[0] == null) {
    return [null, i];
  }
  j = x[1];

  x = munchArrayOfBytes(lines, j);
  if (x[0] == null) {
    entry.response = [];
  } else {
    entry.response = x[0];
  }
  j = x[1];

  parsed_entries.push(entry);

  return [entry, j];
}

function munchNewMessageStart(lines, i, in_flight) {
  let ts = extractUsec(lines[i]);
  let serial = extractSerial(lines[i]);

  let entry = {
    netfn: 0,
    lun: 0,
    cmd: 0,
    serial: -999,
    start_usec: 0,
    end_usec: 0,
    request: [],
    response: []
  };
  entry.start_usec = ts;
  entry.serial = serial;

  let x = munchByte(lines, i + 1);
  if (x[0] == null) {
    return [null, i];
  }
  entry.netfn = x[0];
  let j = x[1];

  x = munchByte(lines, j);
  if (x[0] == null) {
    return [null, i];
  }
  entry.lun = x[0];
  j = x[1];

  x = munchByte(lines, j);
  if (x[0] == null) {
    return [null, i];
  }
  entry.cmd = x[0];
  j = x[1];

  x = munchArrayOfBytes(lines, j);
  if (x[0] == null) {
    entry.request = [];
  }  // Truncated
  entry.request = x[0];
  j = x[1];

  return [entry, j];
}

function munchNewMessageEnd(lines, i, in_flight, parsed_entries) {
  let ts = extractUsec(lines[i]);
  let reply_serial = extractReplySerial(lines[i]);

  let entry = null;
  if (reply_serial in in_flight) {
    entry = in_flight[reply_serial];
    delete in_flight[reply_serial];
  } else {
    return [null, i];
  }

  entry.end_usec = ts;

  let x = munchByte(lines, i + 2);  // Skip "struct {"
  if (x[0] == null) {
    return [null, i];
  }  // NetFN
  if (entry.netfn + 1 != x[0]) {
    return [null, i];
  }
  let j = x[1];

  x = munchByte(lines, j);  // LUN
  if (x[0] == null) {
    return [null, i];
  }
  j = x[1];

  x = munchByte(lines, j);  // CMD
  if (x[0] == null) {
    return [null, i];
  }
  if (entry.cmd != x[0]) {
    return [null, i];
  }
  j = x[1];

  x = munchByte(lines, j);  // cc
  if (x[0] == null) {
    return [null, i];
  }
  j = x[1];

  x = munchArrayOfBytes(lines, j);
  if (x[0] == null) {
    entry.response = [];
  } else {
    entry.response = x[0];
  }
  j = x[1];

  parsed_entries.push(entry);

  return [entry, j];
}

// Parsing state
let g_ipmi_parse_buf = '';
let g_ipmi_parse_lines = [];
let g_ipmi_in_flight = {};
let g_ipmi_parsed_entries = [];
function StartParseIPMIDump() {
  g_ipmi_parse_lines = [];
  g_ipmi_parsed_entries = [];
  g_ipmi_in_flight = {};
  g_ipmi_parse_buf = '';
}
function AppendToParseBuffer(x) {
  g_ipmi_parse_buf += x;
}
function MunchLines() {
  // 1. Extract all lines from the buffer
  let chars_munched = 0;
  while (true) {
    let idx = g_ipmi_parse_buf.indexOf('\n');
    if (idx == -1) break;
    let l = g_ipmi_parse_buf.substr(0, idx);
    g_ipmi_parse_lines.push(l);
    g_ipmi_parse_buf = g_ipmi_parse_buf.substr(idx + 1);
    chars_munched += (idx + 1);
  }
  console.log(chars_munched + ' chars munched');

  // 2. Parse as many lines as possible
  let lidx_last = 0;
  let i = 0;
  while (i < g_ipmi_parse_lines.length) {
    let line = g_ipmi_parse_lines[i];
    if (line.indexOf('interface=org.openbmc.HostIpmi') != -1 &&
        line.indexOf('member=ReceivedMessage') != -1) {
      let x = munchLegacyMessageStart(g_ipmi_parse_lines, i);
      let entry = x[0];
      if (i != x[1]) lidx_last = x[1];  // Munch success!
      i = x[1];
      if (entry != null) {
        g_ipmi_in_flight[entry.serial] = entry;
      }
    } else if (
        line.indexOf('interface=org.openbmc.HostIpmi') != -1 &&
        line.indexOf('member=sendMessage') != -1) {
      let x = munchLegacyMessageEnd(
          g_ipmi_parse_lines, i, g_ipmi_in_flight, g_ipmi_parsed_entries);
      if (i != x[1]) lidx_last = x[1];  // Munch success!
      i = x[1];

    } else if (
        line.indexOf('interface=xyz.openbmc_project.Ipmi.Server') != -1 &&
        line.indexOf('member=execute') != -1) {
      let x = munchNewMessageStart(g_ipmi_parse_lines, i);
      let entry = x[0];
      if (i != x[1]) lidx_last = x[1];
      i = x[1];
      if (entry != null) {
        g_ipmi_in_flight[entry.serial] = entry;
      }
    } else if (line.indexOf('method return') != -1) {
      let x = munchNewMessageEnd(
          g_ipmi_parse_lines, i, g_ipmi_in_flight, g_ipmi_parsed_entries);
      if (i != x[1]) lidx_last = x[1];  // Munch success
      i = x[1];
    }
    i++;
  }
  g_ipmi_parse_lines = g_ipmi_parse_lines.slice(
      lidx_last,
      g_ipmi_parse_lines.length);  // Remove munched lines
  console.log(
      lidx_last + ' lines munched, |lines|=' + g_ipmi_parse_lines.length +
          ', |entries|=' + g_ipmi_parsed_entries.length,
      ', |inflight|=' + Object.keys(g_ipmi_in_flight).length);
}

function UpdateLayout() {
  if (g_ipmi_parsed_entries.length > 0) {
    // Write to Data_IPMI
    let ts0 = g_ipmi_parsed_entries[0].start_usec;
    let ts1 = g_ipmi_parsed_entries[g_ipmi_parsed_entries.length - 1].end_usec;

    // When calling from DBus PCap loader, the following happens
    // >> OnGroupByConditionChanged
    //   >> Preprocess  <-- Time shift will happen here
    // So, we don't do time-shifting here
    let time_shift;
    if (g_StartingSec != undefined) {
      time_shift = BigInt(0);
    } else {  // This is during live capture mode
      time_shift = ts0;
    }

    Data_IPMI = [];
    for (i = 0; i < g_ipmi_parsed_entries.length; i++) {
      let entry = g_ipmi_parsed_entries[i];
      let x = [
        entry.netfn, entry.cmd, parseInt(entry.start_usec - time_shift),
        parseInt(entry.end_usec - time_shift), entry.request, entry.response
      ];
      Data_IPMI.push(x);
    }

    // Re-calculate time range
    RANGE_LEFT_INIT = 0;
    RANGE_RIGHT_INIT =
        parseInt((ts1 - ts0) / BigInt(1000000) / BigInt(10)) * 10 + 10;

    // Perform layout again
    IsCanvasDirty = true;
    OnGroupByConditionChanged();
    BeginSetBoundaryAnimation(RANGE_LEFT_INIT, RANGE_RIGHT_INIT);
    ComputeHistogram();
  } else {
    console.log('No entries parsed');
  }
}

function ParseIPMIDump(data) {
  StartParseIPMIDump();
  AppendToParseBuffer(data);
  MunchLines();
  UpdateLayout();
}