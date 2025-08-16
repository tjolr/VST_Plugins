# 🎵 VST Vibe Coding with AI

**Create professional VST/AU plugins using AI and JUCE framework - no C++ knowledge required!**

This repository contains a complete workflow for building VST plugins using Claude Code AI assistance with the JUCE framework. Whether you want to create an 808 clap, shimmer reverb, or Radio Music-style sampler, this setup handles the complex C++ code while you focus on the creative vision.

## ✨ What You Can Build

- **Effects**: Reverbs, delays, filters, distortion, modulation effects
- **Instruments**: Samplers, synthesizers, drum machines, granular processors
- **Utilities**: Meters, analyzers, MIDI processors, creative tools
- **Experimental**: Novel audio processors, AI-driven effects, unique sound design tools

## 🚀 Quick Start

### Prerequisites

1. **Download JUCE Framework**

   - Visit [https://juce.com/get-juce/download](https://juce.com/get-juce/download)
   - Download and install to `/Applications/JUCE`

2. **Install Claude Code**

   - Follow instructions at [https://docs.anthropic.com/en/docs/claude-code](https://docs.anthropic.com/en/docs/claude-code)
   - Requires Claude subscription ($20+ per month recommended)

3. **Install Dependencies** (macOS)
   ```bash
   brew install ninja
   brew install --cask pluginval
   ```

### Creating Your First Plugin

1. **Clone and navigate to this repository:**

   ```bash
   git clone https://github.com/lexchristopherson/VST-Vibe-Coding.git
   cd VST-Vibe-Coding
   ```

2. **Create a new plugin:**

   ```bash
   ./create.sh "MyAwesomePlugin"
   cd MyAwesomePlugin
   ```

3. **Start Claude Code with bypass permissions:**

   ```bash
   claude --dangerously-skip-permissions
   ```

4. **Run the 4-step AI workflow:**

   ```bash
   /1-juice-research "[describe your plugin idea]"
   /2-juice-specification
   /3-juice-checklist
   /4-juice-build
   ```

5. **Wait for magic to happen!** ✨
   - Claude will research, spec, plan, and build your plugin
   - Automatic testing ensures quality
   - Built plugins are automatically installed to your system

## 🔄 The 4-Step Workflow

### Step 1: Research (`/1-juice-research`)

- **What it does**: Analyzes your plugin idea and researches implementations
- **AI searches**: Academic papers, open-source repos, tutorials, best practices
- **Classifies**: Simple/Advanced/Novel to guide research depth
- **Creates**: `/research/` folder with findings, summaries, and recommendations
- **Example**: `/1-juice-research simple and effective shimmer reverb with modern flat GUI`

### Step 2: Specification (`/2-juice-specification`)

- **What it does**: Creates detailed technical specification from research
- **Analyzes**: All research files and project requirements
- **Creates**: `spec.md` with architecture, requirements, and technical details
- **Includes**: Risk assessment, build feasibility, validation strategy

### Step 3: Checklist (`/3-juice-checklist`)

- **What it does**: Breaks down specification into actionable development tasks
- **Creates**: `checklist.md` with phase-based build plan
- **Uses**: Test-Driven Development (TDD) methodology
- **Ensures**: Each task is atomic and testable

### Step 4: Build (`/4-juice-build`)

- **What it does**: Executes the checklist using strict TDD
- **Process**: Write failing test → Implement minimal code → Pass tests → Refactor
- **Validates**: Continuous testing with pluginval and audio regression tests
- **Installs**: Automatically copies built plugin to system directories

## 🎯 Key Features

### **Automated Quality Assurance**

- **pluginval testing**: Industry-standard plugin validation
- **Audio regression tests**: Ensures DSP behaves correctly
- **CPU performance monitoring**: Prevents real-time audio glitches
- **UI screenshot validation**: Verifies interface renders properly

### **Smart Research System**

- **Context7 integration**: Access to up-to-date JUCE documentation
- **Web research**: Finds relevant papers, tutorials, and implementations
- **License checking**: Avoids GPL conflicts automatically
- **Best practices**: Follows modern JUCE development patterns

### **Professional Build Pipeline**

- **Ninja builds**: Fast, reliable compilation
- **Automatic installation**: Plugins appear in your DAW immediately
- **Git integration**: Every step is committed with clear messages
- **CI/CD ready**: GitHub Actions workflow included

## 🛠️ Project Structure

```
YourPlugin/
├── .claude/           # AI workflow commands
├── research/          # Generated research files
├── Source/           # C++ source code
├── JuceLibraryCode/  # JUCE framework integration
├── Builds/           # Platform-specific build files
├── spec.md           # Technical specification
├── checklist.md      # Development checklist
└── YourPlugin.jucer  # JUCE project file
```

## 💡 Tips for Success

### **Writing Good Plugin Descriptions**

- Be specific about the sound/behavior you want
- Mention GUI preferences (minimal, skeuomorphic, etc.)
- Reference existing plugins for comparison
- Include target use cases

**Good examples:**

- `vintage tape delay with wow/flutter and soft saturation`
- `clean shimmer reverb like Valhalla Shimmer with modern flat GUI`
- `808-style clap with flam control and stereo spread`

### **Iterating and Improving**

- Use `/continue` to resume interrupted builds
- Add features incrementally after core functionality works
- Test in your DAW frequently during development
- Use the research files to understand the implementation

### **Troubleshooting**

- Check `pluginval` output for validation errors
- Review git commits to understand what changed
- Use the generated tests to isolate issues
- Reference JUCE documentation through Context7

## 🎵 Example Plugins Built

- **808 Clap**: Realistic clap sound with flam, decay, and stereo controls
- **Radio Music**: Folder-based sample player with pitch and filtering
- **Shimmer Reverb**: Ethereal reverb with pitch shifting and modulation

## 📝 Advanced Usage

### **Custom Commands**

The `.claude/` directory contains customizable AI prompts. You can modify these for specialized workflows or add new commands for specific plugin types.

### **Testing Strategy**

- Unit tests validate individual components
- Integration tests ensure DSP chain works correctly
- End-to-end tests verify full plugin operation
- Performance tests prevent CPU issues

### **Continuous Integration**

Push to GitHub to trigger automatic builds on multiple platforms. The included CI workflow builds universal binaries and runs all validation tests.

## 🤝 Contributing

This framework is designed to evolve with the community. Share your successful plugins, improve the AI prompts, and contribute new workflows for different plugin types.

## ⚠️ Important Notes

- **Permissions**: The `--dangerously-skip-permissions` flag is used for automation. Only use with trusted code.
- **JUCE License**: Commercial plugins may require JUCE commercial license
- **Claude Costs**: Complex plugins may use significant AI tokens ($20-200/month recommended)
- **Build Time**: Initial builds can take 30-60 minutes depending on complexity

---

**Ready to create your first plugin?** Run `./create.sh "YourPluginName"` and let's start vibe coding! 🎶
