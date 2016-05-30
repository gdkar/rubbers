/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2014 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/
#define __STDC_CONSTANT_MACROS

extern "C" {
#include <portaudio.h>
};
#include "rubbers/RubbersStretcher.h"
#include "rubbers/RubbersFile.h"
#include <iostream>
#include <cmath>
#include <random>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fstream>

#include "system/sysutils.h"

#ifdef __MSVC__
#include "getopt/getopt.h"
#else
#include <getopt.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#include "base/Profiler.h"
using namespace std;
using namespace Rubbers;

#ifdef _WIN32
using Rubbers::gettimeofday;
#endif

#ifdef __MSVC__
using Rubbers::usleep;
#endif

double tempo_convert(const char *str)
{
    char *d = strchr((char *)str, ':');

    if (!d || !*d) {
        double m = atof(str);
        if (m != 0.0) return 1.0 / m;
        else return 1.0;
    }

    char *a = strdup(str);
    char *b = strdup(d+1);
    a[d-str] = '\0';
    double m = atof(a);
    double n = atof(b);
    free(a);
    free(b);
    if (n != 0.0 && m != 0.0) return m / n;
    else return 1.0;
}

int main(int argc, char **argv)
{
    int c;

    double ratio = 1.0;
    double duration = 0.0;
    double pitchshift = 0.0;
    double frequencyshift = 1.0;
    int debug = 0;
    bool realtime = false;
    bool precise = true;
    int threading = 0;
    bool lamination = true;
    bool longwin = false;
    bool shortwin = false;
    bool smoothing = false;
    bool hqpitch = false;
    bool formant = false;
    bool together = false;
    bool crispchanged = false;
    int crispness = -1;
    bool help = false;
    bool version = false;
    bool quiet = false;
    bool haveRatio = false;
    std::string mapfile;
    enum {
        NoTransients,
        BandLimitedTransients,
        Transients
    } transients = Transients;

    enum {
        CompoundDetector,
        PercussiveDetector,
        SoftDetector
    } detector = CompoundDetector;
    while (1) {
        int optionIndex = 0;
        static struct option longOpts[] = {
            { "help",          0, 0, 'h' },
            { "version",       0, 0, 'V' },
            { "time",          1, 0, 't' },
            { "tempo",         1, 0, 'T' },
            { "duration",      1, 0, 'D' },
            { "pitch",         1, 0, 'p' },
            { "frequency",     1, 0, 'f' },
            { "crisp",         1, 0, 'c' },
            { "crispness",     1, 0, 'c' },
            { "debug",         1, 0, 'd' },
            { "realtime",      0, 0, 'R' },
            { "loose",         0, 0, 'L' },
            { "precise",       0, 0, 'P' },
            { "formant",       0, 0, 'F' },
            { "no-threads",    0, 0, '0' },
            { "no-transients", 0, 0, '1' },
            { "no-lamination", 0, 0, '2' },
            { "centre-focus",  0, 0, '7' },
            { "window-long",   0, 0, '3' },
            { "window-short",  0, 0, '4' },
            { "bl-transients", 0, 0, '8' },
            { "detector-perc", 0, 0, '5' },
            { "detector-soft", 0, 0, '6' },
            { "smoothing",     0, 0, '9' },
            { "pitch-hq",      0, 0, '%' },
            { "threads",       0, 0, '@' },
            { "quiet",         0, 0, 'q' },
            { "timemap",       1, 0, 'M' },
            { 0, 0, 0, 0 }
        };

        c = getopt_long(argc, argv, "t:p:d:RLPFc:f:T:D:qhVM:", longOpts, &optionIndex);
        if (c == -1) break;
        switch (c) {
        case 'h': help = true; break;
        case 'V': version = true; break;
        case 't': ratio *= atof(optarg); haveRatio = true; break;
        case 'T': ratio *= tempo_convert(optarg); haveRatio = true; break;
        case 'D': duration = atof(optarg); haveRatio = true; break;
        case 'p': pitchshift = atof(optarg); haveRatio = true; break;
        case 'f': frequencyshift = atof(optarg); haveRatio = true; break;
        case 'd': debug = atoi(optarg); break;
        case 'R': realtime = true; break;
        case 'L': precise = false; break;
        case 'P': precise = true; break;
	case 'F': formant = true; break;
        case '0': threading = 1; break;
        case '@': threading = 2; break;
        case '1': transients = NoTransients; crispchanged = true; break;
        case '2': lamination = false; crispchanged = true; break;
        case '3': longwin = true; crispchanged = true; break;
        case '4': shortwin = true; crispchanged = true; break;
        case '5': detector = PercussiveDetector; crispchanged = true; break;
        case '6': detector = SoftDetector; crispchanged = true; break;
        case '7': together = true; break;
        case '8': transients = BandLimitedTransients; crispchanged = true; break;
        case '9': smoothing = true; crispchanged = true; break;
        case '%': hqpitch = true; break;
        case 'c': crispness = atoi(optarg); break;
        case 'q': quiet = true; break;
        case 'M': mapfile = optarg; break;
        default:  help = true; break;
        }
    }
    if (version) { cerr << RUBBERBAND_VERSION << endl; return 0; }
    if (help || !haveRatio || optind + 1 != argc) {
        cerr << endl;
	cerr << "Rubber Band" << endl;
        cerr << "An audio time-stretching and pitch-shifting library and utility program." << endl;
	cerr << "Copyright 2007-2014 Particular Programs Ltd." << endl;
        cerr << endl;
	cerr << "   Usage: " << argv[0] << " [options] <infile.wav> <outfile.wav>" << endl;
        cerr << endl;
        cerr << "You must specify at least one of the following time and pitch ratio options." << endl;
        cerr << endl;
        cerr << "  -t<X>, --time <X>       Stretch to X times original duration, or" << endl;
        cerr << "  -T<X>, --tempo <X>      Change tempo by multiple X (same as --time 1/X), or" << endl;
        cerr << "  -T<X>, --tempo <X>:<Y>  Change tempo from X to Y (same as --time X/Y), or" << endl;
        cerr << "  -D<X>, --duration <X>   Stretch or squash to make output file X seconds long" << endl;
        cerr << endl;
        cerr << "  -p<X>, --pitch <X>      Raise pitch by X semitones, or" << endl;
        cerr << "  -f<X>, --frequency <X>  Change frequency by multiple X" << endl;
        cerr << endl;
        cerr << "  -M<F>, --timemap <F>    Use file F as the source for key frame map" << endl;
        cerr << endl;
        cerr << "A map file consists of a series of lines each having two numbers separated" << endl;
        cerr << "by a single space.  These are source and target sample frame numbers for fixed" << endl;
        cerr << "time points within the audio data, defining a varying stretch factor through" << endl;
        cerr << "the audio.  You must specify an overall stretch factor using e.g. -t as well." << endl;
        cerr << endl;
        cerr << "The following options provide a simple way to adjust the sound.  See below" << endl;
        cerr << "for more details." << endl;
        cerr << endl;
        cerr << "  -c<N>, --crisp <N>      Crispness (N = 0,1,2,3,4,5,6); default 5 (see below)" << endl;
	cerr << "  -F,    --formant        Enable formant preservation when pitch shifting" << endl;
        cerr << endl;
        cerr << "The remaining options fine-tune the processing mode and stretch algorithm." << endl;
        cerr << "These are mostly included for test purposes; the default settings and standard" << endl;
        cerr << "crispness parameter are intended to provide the best sounding set of options" << endl;
        cerr << "for most situations.  The default is to use none of these options." << endl;
        cerr << endl;
        cerr << "  -L,    --loose          Relax timing in hope of better transient preservation" << endl;
        cerr << "  -P,    --precise        Ignored: The opposite of -L, this is default from 1.6" << endl;
        cerr << "  -R,    --realtime       Select realtime mode (implies --no-threads)" << endl;
        cerr << "         --no-threads     No extra threads regardless of CPU and channel count" << endl;
        cerr << "         --threads        Assume multi-CPU even if only one CPU is identified" << endl;
        cerr << "         --no-transients  Disable phase resynchronisation at transients" << endl;
        cerr << "         --bl-transients  Band-limit phase resync to extreme frequencies" << endl;
        cerr << "         --no-lamination  Disable phase lamination" << endl;
        cerr << "         --window-long    Use longer processing window (actual size may vary)" << endl;
        cerr << "         --window-short   Use shorter processing window" << endl;
        cerr << "         --smoothing      Apply window presum and time-domain smoothing" << endl;
        cerr << "         --detector-perc  Use percussive transient detector (as in pre-1.5)" << endl;
        cerr << "         --detector-soft  Use soft transient detector" << endl;
        cerr << "         --pitch-hq       In RT mode, use a slower, higher quality pitch shift" << endl;
        cerr << "         --centre-focus   Preserve focus of centre material in stereo" << endl;
        cerr << "                          (at a cost in width and individual channel quality)" << endl;
        cerr << endl;
        cerr << "  -d<N>, --debug <N>      Select debug level (N = 0,1,2,3); default 0, full 3" << endl;
        cerr << "                          (N.B. debug level 3 includes audible ticks in output)" << endl;
        cerr << "  -q,    --quiet          Suppress progress output" << endl;
        cerr << endl;
        cerr << "  -V,    --version        Show version number and exit" << endl;
        cerr << "  -h,    --help           Show this help" << endl;
        cerr << endl;
        cerr << "\"Crispness\" levels:" << endl;
        cerr << "  -c 0   equivalent to --no-transients --no-lamination --window-long" << endl;
        cerr << "  -c 1   equivalent to --detector-soft --no-lamination --window-long (for piano)" << endl;
        cerr << "  -c 2   equivalent to --no-transients --no-lamination" << endl;
        cerr << "  -c 3   equivalent to --no-transients" << endl;
        cerr << "  -c 4   equivalent to --bl-transients" << endl;
        cerr << "  -c 5   default processing options" << endl;
        cerr << "  -c 6   equivalent to --no-lamination --window-short (may be good for drums)" << endl;
        cerr << endl;
	return 2;
    }

    if (crispness >= 0 && crispchanged) {
        cerr << "WARNING: Both crispness option and transients, lamination or window options" << endl;
        cerr << "         provided -- crispness will override these other options" << endl;
    }
    switch (crispness) {
    case -1: crispness = 5; break;
    case 0: detector = CompoundDetector; transients = NoTransients; lamination = false; longwin = true; shortwin = false; break;
    case 1: detector = SoftDetector; transients = Transients; lamination = false; longwin = true; shortwin = false; break;
    case 2: detector = CompoundDetector; transients = NoTransients; lamination = false; longwin = false; shortwin = false; break;
    case 3: detector = CompoundDetector; transients = NoTransients; lamination = true; longwin = false; shortwin = false; break;
    case 4: detector = CompoundDetector; transients = BandLimitedTransients; lamination = true; longwin = false; shortwin = false; break;
    case 5: detector = CompoundDetector; transients = Transients; lamination = true; longwin = false; shortwin = false; break;
    case 6: detector = CompoundDetector; transients = Transients; lamination = false; longwin = false; shortwin = true; break;
    };
    if (!quiet) {
        cerr << "Using crispness level: " << crispness << " (";
        switch (crispness) {
        case 0: cerr << "Mushy"; break;
        case 1: cerr << "Piano"; break;
        case 2: cerr << "Smooth"; break;
        case 3: cerr << "Balanced multitimbral mixture"; break;
        case 4: cerr << "Unpitched percussion with stable notes"; break;
        case 5: cerr << "Crisp monophonic instrumental"; break;
        case 6: cerr << "Unpitched solo percussion"; break;
        }
        cerr << ")" << endl;
    }
    std::map<size_t, size_t> mapping;
    if (mapfile != "") {
        std::ifstream ifile(mapfile.c_str());
        if (!ifile.is_open()) { cerr << "ERROR: Failed to open time map file \"" << mapfile << "\"" << endl; return 1; }
        std::string line;
        int lineno = 0;
        while (!ifile.eof()) {
            std::getline(ifile, line);
            while (line.length() > 0 && line[0] == ' ') line = line.substr(1);
            if (line == "") {
                ++lineno;
                continue;
            }
            std::string::size_type i = line.find_first_of(" ");
            if (i == std::string::npos) {
                cerr << "ERROR: Time map file \"" << mapfile << "\" is malformed at line " << lineno << endl;
                return 1;
            }
            size_t source = atoi(line.substr(0, i).c_str());
            while (i < line.length() && line[i] == ' ') ++i;
            size_t target = atoi(line.substr(i).c_str());
            mapping[source] = target;
            if (debug > 0) { cerr << "adding mapping from " << source << " to " << target << endl; }
            ++lineno;
        }
        ifile.close();
        if (!quiet) { cerr << "Read " << mapping.size() << " line(s) from map file" << endl; }
    }
    char *fileName = strdup(argv[optind++]);
    auto rubbersFile = new RubbersFile ( fileName );
    auto length = rubbersFile->length();
    if (duration != 0.0) {
        if (length == 0 || rubbersFile->rate() == 0) {
            cerr << "ERROR: File lacks frame count or sample rate in header, cannot use --duration" << endl;
            return 1;
        }
        auto induration = double(length) / double(rubbersFile->rate());
        if (induration != 0.0) ratio = duration / induration;
    }
    auto ibs = 1<<16;
    auto channels = rubbersFile->channels();
    auto rate = rubbersFile->rate();
    RubbersStretcher::Options options = 0;
    if (realtime)    options |= RubbersStretcher::OptionProcessRealTime;
    if (precise)     options |= RubbersStretcher::OptionStretchPrecise;
    if (!lamination) options |= RubbersStretcher::OptionPhaseIndependent;
    if (longwin)     options |= RubbersStretcher::OptionWindowLong;
    if (shortwin)    options |= RubbersStretcher::OptionWindowShort;
    if (smoothing)   options |= RubbersStretcher::OptionSmoothingOn;
    if (formant)     options |= RubbersStretcher::OptionFormantPreserved;
    if (hqpitch)     options |= RubbersStretcher::OptionPitchHighQuality;
    if (together)    options |= RubbersStretcher::OptionChannelsTogether;
    switch (threading) {
    case 0: options |= RubbersStretcher::OptionThreadingAuto; break;
    case 1: options |= RubbersStretcher::OptionThreadingNever; break;
    case 2: options |= RubbersStretcher::OptionThreadingAlways; break;
    }
    switch (transients) {
    case NoTransients:          options |= RubbersStretcher::OptionTransientsSmooth; break;
    case BandLimitedTransients: options |= RubbersStretcher::OptionTransientsMixed;  break;
    case Transients:            options |= RubbersStretcher::OptionTransientsCrisp;  break;
    }
    switch (detector) {
    case CompoundDetector:      options |= RubbersStretcher::OptionDetectorCompound;   break;
    case PercussiveDetector:    options |= RubbersStretcher::OptionDetectorPercussive; break;
    case SoftDetector:          options |= RubbersStretcher::OptionDetectorSoft;       break;
    }
    if (pitchshift != 0.0) {frequencyshift *= std::pow(2.0, pitchshift / 12);}
    cerr << "Using time ratio " << ratio << " and frequency ratio " << frequencyshift << endl;
    auto start_time = std::chrono::system_clock::now ();

    RubbersStretcher::setDefaultDebugLevel(debug);
    RubbersStretcher ts(rate, channels, options,ratio, frequencyshift);

    ts.setExpectedInputDuration(length);
    auto ibufr = std::make_unique<std::unique_ptr<float[]>[] >(channels);
    auto ibuf = std::make_unique<float*[]>(channels);
    for (size_t i = 0; i < channels; ++i) {
        ibufr[i] = std::make_unique<float[]>(ibs);
        ibuf[i] = ibufr[i].get();
    }
    int frame = 0;
    int percent = 0;
    rubbersFile->seek(0,SEEK_SET);
    if (!realtime) {
        if (!quiet) {cerr << "Pass 1: Studying..." << endl;}
        while (frame < length) {
            int count = -1;
            count = rubbersFile->read(ibuf.get(),ibs );
            bool final = (frame + ibs >= length);
            ts.study(ibuf.get(), count, final);
            int p = int((double(frame) * 100.0) / length);
            if (p > percent || frame == 0) {
                percent = p;
                if (!quiet) {
                    cerr << "\r" << percent << "% ";
                }
            }
            frame += count;
        }
        if (!quiet) {cerr << "\rCalculating profile..." << endl;}
        rubbersFile->seek(0,SEEK_SET);
    }
    frame = 0;
    percent = 0;
    if (!mapping.empty()) {ts.setKeyFrameMap(mapping);}
    size_t countIn = 0, countOut = 0;
    while (frame < length) {
        int count = rubbersFile->read(ibuf.get(),ibs);
        bool final = (frame + ibs >= length);
        if (debug > 2) {
            cerr << "count = " << count << ", ibs = " << ibs << ", frame = " << frame << ", frames = " << length << ", final = " << final << endl;
        }
        ts.process(ibuf.get(), count, final);
        countIn += count;
        int avail = ts.available();
        if (debug > 1) cerr << "available = " << avail << endl;
        if (avail > 0) {
            float **obf = new float *[channels];
            for (auto i = 0; i < channels; ++i) {obf[i] = new float[avail];}
            ts.retrieve(obf, avail);
            countOut += avail;
            float *fobf = new float[channels * avail];
            for (size_t c = 0; c < channels; ++c) {
                for (int i = 0; i < avail; ++i) {
                    float value = obf[c][i];
                    if (value > 1.f) value = 1.f;
                    if (value < -1.f) value = -1.f;
                    fobf[i * channels + c] = value;
                }
            }
            fwrite ( fobf, sizeof(float), avail * channels, stdout);
            delete[] fobf;
            for (size_t i = 0; i < channels; ++i) {delete[] obf[i];}
            delete[] obf;
        }
        if (frame == 0 && !realtime && !quiet) {cerr << "Pass 2: Processing..." << endl;}
	int p = int((double(frame) * 100.0) / length);
	if (p > percent || frame == 0) {
	    percent = p;
            if (!quiet) {cerr << "\r" << percent << "% ";}
	}
        frame += ibs;
    }
    if (!quiet) {cerr << "\r    " << endl;}
    int avail;
    while ((avail = ts.available()) >= 0) {
        if (debug > 1) {cerr << "(completing) available = " << avail << endl;}
        if (avail > 0) {
            auto obf = new float *[channels];
            for (auto i = 0; i < channels; ++i) {obf[i] = new float[avail];}
            ts.retrieve(obf, avail);
            countOut += avail;
            float *fobf = new float[channels * avail];
            for (size_t c = 0; c < channels; ++c) {
                for (int i = 0; i < avail; ++i) {
                    float value = obf[c][i];
                    if (value > 1.f) value = 1.f;
                    if (value < -1.f) value = -1.f;
                    fobf[i * channels + c] = value;
                }
            }
//            sf_writef_float(sndfileOut, fobf, avail);
            delete[] fobf;
            delete[] obf;
        } else {usleep(10000);}
    }
    if (!quiet) {
        cerr << "in: " << countIn << ", out: " << countOut << ", ratio: " << double(countOut)/double(countIn) << ", ideal output: " << lrint(countIn * ratio) << ", error: " << abs(lrint(countIn * ratio) - int(countOut)) << endl;
        auto end_time = std::chrono::system_clock::now ();
        auto duration = static_cast<std::chrono::duration<double,std::chrono::seconds::period> >( end_time-start_time );
        cerr << "elapsed time: " << duration.count() << " sec, in frames/sec: " << countIn/duration.count() << ", out frames/sec: " << countOut/duration.count() << endl;
    }
    Rubbers::Profiler::dump();
    return 0;
}
