const {spawn} = require('child_process');
const targz = require('targz');

// Capture state for all scripts
var g_capture_state = 'not started';
var g_capture_mode = 'live';

function currTimestamp() {
  var tmp = new Date();
  return (tmp.getTime() + tmp.getTimezoneOffset() * 60000) / 1000;
}

var g_child;
var g_rz;

var g_capture_live = true;
var g_dbus_capture_tarfile_size = 0;

// Delimiters: ">>>>>>" and "<<<<<<"
function ExtractMyDelimitedStuff(x, parse_as = undefined) {
  let i0 = x.lastIndexOf('>>>>>>'), i1 = x.lastIndexOf('<<<<<<');
  if (i0 != -1 && i1 != -1) {
    let ret = x.substr(i0 + 6, i1 - i0 - 6);
    if (parse_as == undefined)
      return ret;
    else if (parse_as == 'int')
      return parseInt(ret);
  } else
    return null;
}

function streamWrite(stream, chunk, encoding = 'utf8') {
  return new Promise((resolve, reject) => {
    const errListener = (err) => {
      stream.removeListener('error', errListener);
      reject(err);
    };
    stream.addListener('error', errListener);
    const callback = () => {
      stream.removeListener('error', errListener);
      resolve(undefined);
    };
    stream.write(chunk, encoding, callback);
  });
}

function ExtractTarFile() {
  const tar_file = 'DBUS_MONITOR.tar.gz';
  const target = '.';

  targz.decompress({src: tar_file, dest: target}, function(err) {
    if (err) {
      console.log('Error decompressing .tar.gz file:' + err);
    } else {
      console.log('Done! will load file contents');
      fs.readFile(
          './DBUS_MONITOR',
          {
            encoding: 'utf-8'
          },
          (err, data) => {
            if (err) {
              console.log('Error in readFile: ' + err);
            } else {
              ParseIPMIDump(data);
            }
          });
    }
  });
}

async function OnTransferCompleted() {
  setTimeout(function() {
    g_rz.kill('SIGINT');
    g_child.kill('SIGINT');
  }, 5000);

  capture_info.textContent = 'Loaded the capture file';
  OnCaptureStop();
  ExtractTarFile();
}

function IsLast10CharsSame(data, ch) {
  let ok = tr;
  for (let i = data.length - 1, j = 0; i >= 0; i--, j++) {
  }
}
function MyCorrection(data) {}

// Example output from stderr:
// ^M Bytes received:    2549/   2549   BPS:6370
async function LaunchRZ() {
  // On the Host

  // Remove existing file
  const file_names = ['DBUS_MONITOR', 'DBUS_MONITOR.tar.gz'];
  try {
    for (let i = 0; i < 2; i++) {
      const fn = file_names[i];
      if (fs.existsSync(fn)) {
        fs.unlinkSync(fn);  // unlink is basically rm
        console.log('Removed file: ' + fn);
      }
    }
  } catch (err) {
  }

  g_rz = spawn('rz', ['-v', '-a', '-e', '-E', '-w', '32767'], {shell: true});
  g_rz.stdout.on('data', (data) => {
    console.log('[rz] received ' + data.length + ' B');
    console.log(data);
    g_child.stdin.write(data);
  });
  g_rz.stderr.on('data', (data) => {
    console.log('[rz] error: ' + data);
    let s = data.toString();
    let idx = s.lastIndexOf('Bytes received:');
    if (idx != -1) {
      capture_info.textContent = s.substr(idx);
    }
    if (data.indexOf('Transfer complete') != -1) {
      OnTransferCompleted();
    } else if (data.indexOf('Transfer incomplete') != -1) {
      // todo: retry transfer
      // Bug info
      // Uncaught Error [ERR_STREAM_WRITE_AFTER_END]: write after end
      // at writeAfterEnd (_stream_writable.js:253)
      // at Socket.Writable.write (_stream_writable.js:302)
      // at Socket.<anonymous> (ipmi_capture.js:317)
      // at Socket.emit (events.js:210)
      // at addChunk (_stream_readable.js:308)
      // at readableAddChunk (_stream_readable.js:289)
      // at Socket.Readable.push (_stream_readable.js:223)
      // at Pipe.onStreamRead (internal/stream_base_commons.js:182)
      capture_info.textContent = 'Transfer incomplete';
    }
  });
  await Promise.all(
      [g_rz.stdin.pipe(g_child.stdout), g_rz.stdout.pipe(g_child.stdin)]);
}

function ClearAllPendingTimeouts() {
  var id = setTimeout(function() {}, 0);
  for (; id >= 0; id--) clearTimeout(id);
}

function StartDbusMonitorFileSizePollLoop() {
  QueueDbusMonitorFileSize(5);
}

function QueueDbusMonitorFileSize(secs = 5) {
  setTimeout(function() {
    g_child.stdin.write(
        'a=`ls -l DBUS_MONITOR | awk \'{print $5}\'` ; echo ">>>>>>$a<<<<<<"  \n\n\n\n');

    QueueDbusMonitorFileSize(secs);
  }, secs * 1000);
}

function StopCapture() {
  switch (g_capture_mode) {
    case 'live':
      g_child.stdin.write('\x03 ');
      g_capture_state = 'stopping';
      capture_info.textContent = 'Ctrl+C sent to BMC console';
      break;
    case 'staged':
      ClearAllPendingTimeouts();
      g_child.stdin.write(
          'echo ">>>>>>" && killall dbus-monitor && echo "<<<<<<" \n\n\n\n');
      g_capture_state = 'stopping';
      capture_info.textContent = 'Stopping dbus-monitor';
      break;
  }
}

function QueueBMCConsoleHello(secs = 3) {
  setTimeout(function() {
    try {
      if (g_capture_state == 'not started') {
        console.log('Sending hello <cr> to the BMC');
        g_child.stdin.write('\n');
        QueueBMCConsoleHello(secs);
      }
    } catch (err) {
      console.log('g_child may have ended as intended');
    }
  }, secs * 1000);
}

// The command line needed to access the BMC. The expectation is
// executing this command brings the user to the BMC's console.
function GetCMDLine() {
  let v = text_hostname.value.split(' ');
  return [v[0], v.slice(1, v.length)];
}

async function StartCapture(host) {
  // Disable buttons
  HideWelcomeScreen();
  ShowIPMITimeline();
  let args = GetCMDLine();
  btn_start_capture.disabled = true;
  select_capture_mode.disabled = true;
  text_hostname.disabled = true;
  capture_info.textContent = 'Contacting BMC console: ' + args.toString();

  // On the B.M.C.
  let last_t = currTimestamp();
  let attempt = 0;
  console.log('Args: ' + args);
  g_child = spawn(args[0], args[1], {shell: true});
  g_child.stdout.on('data', async function(data) {
    QueueBMCConsoleHello();

    var t = currTimestamp();
    {
      switch (g_capture_state) {
        case 'not started':
          attempt++;
          console.log('attempt ' + attempt);
          g_child.stdin.write('echo "haha" \n');
          await streamWrite(g_child.stdin, 'whoami \n');
          let idx = data.indexOf('haha');
          if (idx != -1) {
            ClearAllPendingTimeouts();
            OnCaptureStart();  // Successfully logged on, start
            g_capture_state = 'dbus monitor start';
            capture_info.textContent = 'Reached BMC console';
          } else {
            console.log('idx=' + idx);
          }
          break case 'dbus monitor start': if (g_capture_mode == 'live') {
            // It would be good to make sure the console bit rate is greater
            // than
            //   the speed outputs are generated.
            //            g_child.stdin.write("dbus-monitor --system | grep
            //            \"sendMessage\\|ReceivedMessage\" -A7 \n")
            ClearAllPendingTimeouts();
            g_child.stdin.write(
                'dbus-monitor --system | grep "member=execute\\|method return" -A7 \n');
            capture_info.textContent = 'Started dbus-monitor for live capture';
          }
          else {
            //            g_child.stdin.write("dbus-monitor --system | grep
            //            \"sendMessage\\|ReceivedMessage\" -A7 >
            //            /run/initramfs/DBUS_MONITOR & \n\n\n")
            ClearAllPendingTimeouts();
            g_child.stdin.write(
                'dbus-monitor --system > /run/initramfs/DBUS_MONITOR & \n\n\n');
            StartDbusMonitorFileSizePollLoop();
            capture_info.textContent =
                'Started dbus-monitor for staged capture';
          }
          g_capture_state = 'dbus monitor running';
          break;
        case 'dbus monitor running':
          if (g_capture_mode == 'staged') {
            let s = data.toString();
            let tmp = ExtractMyDelimitedStuff(s, 'int');
            if (tmp != undefined) {
              let sz = Math.floor(parseInt(tmp) / 1024);
              if (!isNaN(sz)) {
                capture_info.textContent =
                    'Raw Dbus capture size: ' + sz + ' KiB';
              } else {  // This can happen if the output is cut by half & may be
                        // fixed by queuing console outputs
              }
            }
          }
          AppendToParseBuffer(data.toString());
          MunchLines();
          UpdateLayout();
          ComputeHistogram();
          break case 'dbus monitor end':  // Todo: add speed check
              let s = data.toString();
          let i0 = s.lastIndexOf('>>>>'), i1 = s.lastIndexOf('<<<<');
          if (i0 != -1 && i1 != -1) {
            let tmp = s.substr(i0 + 4, i1 - i0 - 4);
            let sz = parseInt(tmp);
            if (isNaN(sz)) {
              console.log(
                  'Error: the tentative dbus-profile dump is not found!');
            } else {
              let bps = sz / 10;
              console.log('dbus-monitor generates ' + bps + 'B per second');
            }
          }
          g_child.kill('SIGINT');
          break;
        case 'sz sending':
          console.log('Received a chunk of size ' + data.length);
          capture_info.textContent = data;
          console.log(data);
          //          capture_info.textContent = "Received a chunk of size " +
          //          data.length
          g_rz.stdin.write(data);
          break;
        case 'stopping':
          let t = data.toString();
          if (g_capture_mode == 'live') {
            if (t.lastIndexOf('^C') != -1) {
              // Live mode
              g_child.kill('SIGINT');
              g_capture_state = 'not started';
              OnCaptureStop();
              capture_info.textContent = 'connection to BMC closed';
              // Log mode
            }
          } else if (g_capture_mode == 'staged') {
            ClearAllPendingTimeouts();
            if (t.lastIndexOf('<<<<<<') != -1) {
              g_capture_state = 'compressing';
              g_child.stdin.write(
                  'echo ">>>>>>" && cd /run/initramfs && tar cfz DBUS_MONITOR.tar.gz DBUS_MONITOR && echo "<<<<<<" \n\n\n\n');
              capture_info.textContent = 'Compressing dbus monitor dump on BMC';
            }
          }
          break;
        case 'compressing':
          g_child.stdin.write(
              'a=`ls -l /run/initramfs/DBUS_MONITOR.tar.gz | awk \'{print $5}\'` && echo ">>>>>>$a<<<<<<"   \n\n\n\n');
          g_capture_state = 'dbus_monitor size';
          capture_info.textContent = 'Obtaining size of compressed dbus dump';
          break;
        case 'dbus_monitor size':
          // Starting RZ
          let tmp = ExtractMyDelimitedStuff(data.toString(), 'int');
          console.log('dbus_monitor size tmp=' + tmp);
          if (tmp != undefined) {
            g_dbus_capture_tarfile_size = tmp;
            capture_info.textContent =
                'Starting rz and sz, file size: ' + Math.floor(tmp / 1024) +
                ' KiB';
          } else {
            capture_info.textContent = 'Starting rz and sz';
          }
          g_capture_state = 'sz start';
          g_child.stdin.write(
              'sz -a -e -w 32767 -y /run/initramfs/DBUS_MONITOR.tar.gz\n');
          g_capture_state = 'sz sending';
          LaunchRZ();
          break;
      }
      last_t = t;
    }
  });
  g_child.stderr.on('data', (data) => {
    console.log('[bmc] err=' + data);
    g_child.stdin.write('echo "haha" \n\n');
  });
  g_child.on('close', (code) => {
    console.log('return code: ' + code);
  });
}
