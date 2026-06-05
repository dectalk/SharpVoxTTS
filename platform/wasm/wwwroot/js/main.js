import './AudioPlayer.js';

const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });

let _pendingPlayAt = null;
let _glotMono = null;
let _glotSrcRate = 0;
let _pendingCustomString = null;

worker.onmessage = ({ data }) => {
    switch (data.type) {
        case 'ready': {
            const activeTab = document.querySelector('.tab-btn.active');
            if (activeTab) {
                const mode = activeTab.id.replace('tab-', '');
                worker.postMessage({ type: 'init', klattsch: mode === 'klattsch' || mode === 'tools' });
            }
            window.dispatchEvent(new Event('sharpvox-ready'));
            break;
        }
        case 'initAudio':
            window.initAudio(data.sr);
            _pendingPlayAt = window.reserveStartTime(data.sr);
            break;
        case 'playPcm':
            window.playAudioStream(data.pcm, data.sr);
            break;
        case 'stopAudio':
            window.stopAudio();
            _pendingPlayAt = null;
            break;
        case 'stopPhonemeTracking':
            window.stopPhonemeTracking();
            break;
        case 'startPhonemeTracking':
            window.startPhonemeTracking(data.codes, data.times, _pendingPlayAt ?? 0);
            _pendingPlayAt = null;
            break;
        case 'updateStatus':
            window.ui?.updateStatus(data.msg);
            break;
        case 'updatePhonemes':
            window.ui?.updatePhonemes(data.json, data.idx);
            break;
        case 'updateAllParams':
            window.ui?.updateAllParams(data.json);
            break;
        case 'downloadBytes':
            window.downloadBytes(data.data, data.filename, data.mime);
            break;
        case 'downloadFile':
            window.downloadFile(data.filename, data.content);
            break;
        case 'customString':
            if (_pendingCustomString) { _pendingCustomString(data.value); _pendingCustomString = null; }
            break;
        case 'startVideoExport':
            window.ui?.startVideoExport(
                data.pcm, data.sr,
                data.eventsJson, data.timesJson, data.wordTimesJson,
                data.duration, data.sourceText,
                data.lipsyncTimesJson, data.lipsyncV1Json, data.lipsyncV2Json);
            break;
    }
};

window.sharpVox = {
    Speak:           (text)                        => worker.postMessage({ type: 'Speak', text }),
    StopBtn:         ()                            => worker.postMessage({ type: 'StopBtn' }),
    SetMode:         (klattsch)                    => worker.postMessage({ type: 'SetMode', klattsch }),
    UpdateParam:     (name, value)                 => worker.postMessage({ type: 'UpdateParam', name, value: String(value) }),
    OnPresetChange:  (val)                         => worker.postMessage({ type: 'OnPresetChange', val }),
    HandleImport:    (json)                        => worker.postMessage({ type: 'HandleImport', json }),
    ExportPreset:    ()                            => worker.postMessage({ type: 'ExportPreset' }),
    GetCustomString: ()                            => new Promise(resolve => { _pendingCustomString = resolve; worker.postMessage({ type: 'GetCustomString' }); }),
    DownloadWav:     (text)                        => worker.postMessage({ type: 'DownloadWav', text }),
    AuditionPhoneme: (code)                        => worker.postMessage({ type: 'AuditionPhoneme', code }),
    ExportVideo:     (text)                        => worker.postMessage({ type: 'ExportVideo', text }),
    SetGlottalSample: async (file, naturalPitchHz) => {
        const arrayBuf = await file.arrayBuffer();
        const audioCtx = new OfflineAudioContext(1, 1, 44100);
        const decoded  = await audioCtx.decodeAudioData(arrayBuf);
        // Mix down to mono by averaging all channels
        const numCh = decoded.numberOfChannels;
        const len   = decoded.length;
        const mono  = new Float32Array(len);
        for (let ch = 0; ch < numCh; ch++) {
            const ch_data = decoded.getChannelData(ch);
            for (let i = 0; i < len; i++) mono[i] += ch_data[i];
        }
        if (numCh > 1) { for (let i = 0; i < len; i++) mono[i] /= numCh; }
        _glotMono = mono;
        _glotSrcRate = decoded.sampleRate;
        const copy = mono.slice();
        worker.postMessage({ type: 'SetGlottalSample', pcm: copy, srcRate: _glotSrcRate, naturalPitchHz }, [copy.buffer]);
    },
    UpdateGlotPitch: (naturalPitchHz) => {
        if (!_glotMono) return;
        const copy = _glotMono.slice();
        worker.postMessage({ type: 'SetGlottalSample', pcm: copy, srcRate: _glotSrcRate, naturalPitchHz }, [copy.buffer]);
    },
    ClearGlottalSample: () => {
        _glotMono = null; _glotSrcRate = 0;
        worker.postMessage({ type: 'ClearGlottalSample' });
    },
};
