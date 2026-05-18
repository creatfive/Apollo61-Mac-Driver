import React, { useState, useEffect } from 'react';
import { 
  Keyboard, 
  Zap, 
  Settings, 
  Layers, 
  Cpu, 
  Battery, 
  Wifi, 
  Sun, 
  Moon, 
  Palette, 
  Command, 
  CheckCircle2, 
  AlertCircle,
  ChevronRight,
  Save,
  RotateCcw,
  Sliders
} from 'lucide-react';
import { motion, AnimatePresence } from 'motion/react';
import { HexColorPicker } from 'react-colorful';
import { cn } from './lib/utils';

// --- Types ---
type View = 'dashboard' | 'lighting' | 'mapping' | 'macros' | 'settings';

interface KeyConfig {
  id: string;
  label: string;
  mapping: string;
  color: string;
}

// --- Components ---

const SidebarItem = ({ 
  icon: Icon, 
  label, 
  active, 
  onClick 
}: { 
  icon: any, 
  label: string, 
  active: boolean, 
  onClick: () => void 
}) => (
  <button
    onClick={onClick}
    className={cn(
      "w-full flex items-center gap-3 px-4 py-3 rounded-xl transition-all duration-200 group",
      active 
        ? "bg-white/10 text-white shadow-xl" 
        : "text-white/40 hover:text-white/80 hover:bg-white/5"
    )}
  >
    <Icon className={cn(
      "w-5 h-5 transition-transform duration-200",
      active ? "scale-110" : "group-hover:scale-110"
    )} />
    <span className="font-medium text-sm tracking-tight">{label}</span>
  </button>
);

const SectionTitle = ({ children, subtitle }: { children: React.ReactNode, subtitle?: string }) => (
  <div className="mb-8">
    <h2 className="text-2xl font-semibold tracking-tight text-white mb-1">{children}</h2>
    {subtitle && <p className="text-white/40 text-sm tracking-wide">{subtitle}</p>}
  </div>
);

const Card = ({ children, className }: { children: React.ReactNode, className?: string }) => (
  <div className={cn("bg-white/[0.03] border border-white/[0.08] rounded-2xl p-6 backdrop-blur-xl shadow-2xl", className)}>
    {children}
  </div>
);

// --- Keyboard Layout Component ---
const KeyboardLayout = ({ onKeyClick, configs }: { onKeyClick: (id: string) => void, configs: KeyConfig[] }) => {
  const rows = [
    ['esc', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6', 'f7', 'f8', 'f9', 'f10', 'f11', 'f12', 'del'],
    ['`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'backspace'],
    ['tab', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'],
    ['caps', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", 'enter'],
    ['shift', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 'shift '],
    ['ctrl', 'opt', 'cmd', 'space', 'cmd ', 'opt ', 'left', 'up', 'down', 'right']
  ];

  return (
    <div className="flex flex-col gap-1.5 p-4 bg-black/40 rounded-3xl border border-white/10 shadow-inner">
      {rows.map((row, i) => (
        <div key={i} className="flex gap-1.5 justify-center">
          {row.map((key) => {
            const config = configs.find(c => c.label === key);
            const isWide = ['backspace', 'tab', 'caps', 'enter', 'shift', 'shift '].includes(key);
            const isSpace = key === 'space';
            
            return (
              <motion.button
                key={key}
                whileHover={{ scale: 1.05, y: -2 }}
                whileTap={{ scale: 0.95 }}
                onClick={() => onKeyClick(key)}
                className={cn(
                  "h-10 rounded-md text-[10px] font-bold uppercase transition-all border border-white/5",
                  isWide ? "w-20" : isSpace ? "w-64" : "w-10",
                  "flex items-center justify-center relative shadow-lg",
                  config ? "bg-white/10 text-white" : "bg-white/5 text-white/40"
                )}
                style={{ 
                  boxShadow: config?.color ? `0 0 12px ${config.color}44 inset, 0 0 8px ${config.color}22` : undefined,
                  borderColor: config?.color ? `${config.color}66` : undefined,
                  color: config?.color || undefined
                }}
              >
                {key.replace(' ', '')}
                {config && (
                  <div 
                    className="absolute bottom-1 w-1 h-1 rounded-full animate-pulse" 
                    style={{ backgroundColor: config.color }} 
                  />
                )}
              </motion.button>
            );
          })}
        </div>
      ))}
    </div>
  );
};

// --- Main App Component ---

export default function App() {
  const [activeView, setActiveView] = useState<View>('dashboard');
  const [selectedColor, setSelectedColor] = useState('#ff0066');
  const [brightness, setBrightness] = useState(15);
  const [speed, setSpeed] = useState(10);
  const [activeEffect, setActiveEffect] = useState('Static');
  const [isRainbow, setIsRainbow] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [activeProfile, setActiveProfile] = useState('Default (Gaming)');
  
  const [keyConfigs, setKeyConfigs] = useState<KeyConfig[]>([
    { id: '1', label: 'W', mapping: 'W', color: '#ff0066' },
    { id: '2', label: 'A', mapping: 'A', color: '#ff0066' },
    { id: '3', label: 'S', mapping: 'S', color: '#ff0066' },
    { id: '4', label: 'D', mapping: 'D', color: '#ff0066' },
    { id: '5', label: 'space', mapping: 'Jump', color: '#00f2ff' },
  ]);

  const effectToMode: Record<string, number> = {
    'Static': 1, 'Reactive': 2, 'Single Off': 3, 'Glittering': 4,
    'Falling': 5, 'Colourful': 6, 'Breathing': 7, 'Spectrum': 8,
    'Outward': 9, 'Scrolling': 10, 'Rolling': 11, 'Rotating': 12,
    'Explode': 13, 'Launch': 14, 'Ripples': 15, 'Flowing': 16,
    'Pulsating': 17, 'Tilt': 18, 'Shuttle': 19
  };

  const handleSave = () => {
    setIsSaving(true);
    
    // Parse Hex to RGB
    const r = parseInt(selectedColor.slice(1, 3), 16);
    const g = parseInt(selectedColor.slice(3, 5), 16);
    const b = parseInt(selectedColor.slice(5, 7), 16);
    const modeId = effectToMode[activeEffect] || 1;
    const rainbowVal = isRainbow ? 1 : 0;

    // @ts-ignore
    if (window.pywebview) {
      // @ts-ignore
      window.pywebview.api.apply_settings(modeId, r, g, b, brightness, speed, rainbowVal)
        .then(() => setIsSaving(false))
        .catch(() => setIsSaving(false));
    } else {
      setTimeout(() => setIsSaving(false), 1500);
    }
  };

  const renderContent = () => {
    switch (activeView) {
      case 'dashboard':
        return (
          <motion.div 
            key="dashboard"
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            className="space-y-6"
          >
            <SectionTitle subtitle="Device monitoring & quick actions">Dashboard</SectionTitle>
            
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              <Card className="flex flex-col justify-between">
                <div className="flex justify-between items-start mb-8">
                  <div className="w-12 h-12 rounded-2xl bg-cyan-500/20 flex items-center justify-center">
                    <Battery className="w-6 h-6 text-cyan-400" />
                  </div>
                  <span className="text-white/20 text-xs font-mono uppercase tracking-widest">Power</span>
                </div>
                <div>
                  <h3 className="text-4xl font-light text-white mb-2 tracking-tighter">84%</h3>
                  <p className="text-white/40 text-sm">Est. 12 hours remaining</p>
                </div>
              </Card>

              <Card className="flex flex-col justify-between">
                <div className="flex justify-between items-start mb-8">
                  <div className="w-12 h-12 rounded-2xl bg-purple-500/20 flex items-center justify-center">
                    <Wifi className="w-6 h-6 text-purple-400" />
                  </div>
                  <span className="text-white/20 text-xs font-mono uppercase tracking-widest">Connectivity</span>
                </div>
                <div>
                  <h3 className="text-2xl font-light text-white mb-2 tracking-tight">2.4GHz Wireless</h3>
                  <p className="text-white/40 text-sm">Low Latency Mode Active</p>
                </div>
              </Card>

              <Card className="flex flex-col justify-between border-cyan-500/30 bg-cyan-500/5">
                <div className="flex justify-between items-start mb-8">
                  <div className="w-12 h-12 rounded-2xl bg-emerald-500/20 flex items-center justify-center">
                    <Layers className="w-6 h-6 text-emerald-400" />
                  </div>
                  <span className="text-white/20 text-xs font-mono uppercase tracking-widest">Active Profile</span>
                </div>
                <div>
                  <h3 className="text-2xl font-light text-white mb-2 tracking-tight">{activeProfile}</h3>
                  <button className="text-emerald-400 text-xs font-medium hover:underline flex items-center gap-1 group">
                    Manage Profiles <ChevronRight className="w-3 h-3 group-hover:translate-x-1 transition-transform" />
                  </button>
                </div>
              </Card>
            </div>

            <Card className="p-8">
              <div className="flex items-center justify-between mb-6">
                <h3 className="text-lg font-medium text-white">Latest Firmware</h3>
                <span className="px-3 py-1 bg-emerald-500/10 text-emerald-400 text-[10px] font-bold uppercase tracking-widest rounded-full border border-emerald-500/20">Up to date</span>
              </div>
              <div className="flex gap-4">
                <div className="flex-1 bg-white/[0.02] border border-white/5 rounded-xl p-4">
                  <p className="text-white/60 text-xs mb-1 uppercase tracking-wider font-semibold">Version</p>
                  <p className="text-white text-sm font-mono tracking-tight">v2.4.12-Stable</p>
                </div>
                <div className="flex-1 bg-white/[0.02] border border-white/5 rounded-xl p-4">
                  <p className="text-white/60 text-xs mb-1 uppercase tracking-wider font-semibold">Poll Rate</p>
                  <p className="text-white text-sm font-mono tracking-tight">8000 Hz</p>
                </div>
              </div>
            </Card>
          </motion.div>
        );

      case 'lighting':
        return (
          <motion.div 
            key="lighting"
            initial={{ opacity: 0, scale: 0.98 }}
            animate={{ opacity: 1, scale: 1 }}
            exit={{ opacity: 0, scale: 0.98 }}
            className="space-y-6"
          >
            <SectionTitle subtitle="Customize zones and dynamic effects">Lighting Studio</SectionTitle>
            
            <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
              <div className="lg:col-span-4 space-y-6">
                <Card className="p-0 overflow-hidden">
                  <div className="bg-white/5 p-4 border-b border-white/10 flex items-center justify-between">
                    <span className="text-xs font-bold uppercase tracking-widest text-white/60">Color Wheel</span>
                    <Palette className="w-4 h-4 text-white/40" />
                  </div>
                  <div className="p-6 flex justify-center">
                    <HexColorPicker color={selectedColor} onChange={setSelectedColor} className="!w-full max-w-[240px]" />
                  </div>
                  <div className="px-6 pb-6 space-y-4">
                    <div className="flex items-center gap-3">
                      <div className="w-10 h-10 rounded-lg shadow-lg" style={{ backgroundColor: selectedColor }} />
                      <div className="flex-1">
                        <p className="text-[10px] uppercase font-bold text-white/40 tracking-wider">Hex Value</p>
                        <p className="text-white font-mono text-sm uppercase">{selectedColor}</p>
                      </div>
                    </div>
                  </div>
                </Card>

                <Card>
                  <div className="flex items-center justify-between mb-4">
                    <span className="text-xs font-bold uppercase tracking-widest text-white/60">Brightness</span>
                    <Sun className="w-4 h-4 text-white/40" />
                  </div>
                  <input 
                    type="range" 
                    min="0" max="15"
                    value={brightness}
                    onChange={(e) => setBrightness(parseInt(e.target.value))}
                    className="w-full h-1.5 bg-white/10 rounded-lg appearance-none cursor-pointer accent-white"
                  />
                  <div className="flex justify-between mt-2">
                    <span className="text-[10px] text-white/20 font-mono">0</span>
                    <span className="text-[10px] text-white/60 font-mono">{Math.round((brightness/15)*100)}%</span>
                    <span className="text-[10px] text-white/20 font-mono">15</span>
                  </div>
                </Card>

                <Card>
                  <div className="flex items-center justify-between mb-4">
                    <span className="text-xs font-bold uppercase tracking-widest text-white/60">Speed</span>
                    <Zap className="w-4 h-4 text-white/40" />
                  </div>
                  <input 
                    type="range" 
                    min="0" max="10"
                    value={speed}
                    onChange={(e) => setSpeed(parseInt(e.target.value))}
                    className="w-full h-1.5 bg-white/10 rounded-lg appearance-none cursor-pointer accent-white"
                  />
                  <div className="flex justify-between mt-2">
                    <span className="text-[10px] text-white/20 font-mono">0</span>
                    <span className="text-[10px] text-white/60 font-mono">{Math.round((speed/10)*100)}%</span>
                    <span className="text-[10px] text-white/20 font-mono">10</span>
                  </div>
                </Card>

                <div 
                  onClick={() => setIsRainbow(!isRainbow)}
                  className="flex items-center gap-3 p-4 rounded-xl border border-white/5 bg-white/[0.02] cursor-pointer hover:bg-white/5 transition-colors"
                >
                  <div className={cn("w-5 h-5 rounded border flex items-center justify-center transition-colors", isRainbow ? "bg-cyan-500 border-cyan-500" : "border-white/20")}>
                    {isRainbow && <CheckCircle2 className="w-3 h-3 text-black" />}
                  </div>
                  <span className="text-sm font-medium text-white/80">Force Rainbow Cycle</span>
                </div>
              </div>

              <div className="lg:col-span-8 space-y-6">
                <Card>
                  <h3 className="text-xs font-bold uppercase tracking-widest text-white/60 mb-6">Effect Presets</h3>
                  <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
                    {[
                      { name: 'Static', icon: Sun },
                      { name: 'Reactive', icon: Zap },
                      { name: 'Breathing', icon: Moon },
                      { name: 'Ripples', icon: Sliders },
                      { name: 'Spectrum', icon: Palette },
                      { name: 'Pulsating', icon: AlertCircle },
                      { name: 'Flowing', icon: Cpu },
                      { name: 'Colourful', icon: Command },
                    ].map((effect) => (
                      <button 
                        key={effect.name} 
                        onClick={() => setActiveEffect(effect.name)}
                        className={cn(
                          "flex flex-col items-center gap-3 p-4 rounded-xl border transition-all duration-200 group",
                          activeEffect === effect.name 
                            ? "bg-white/10 border-white/20 text-white" 
                            : "bg-white/[0.02] border-white/5 text-white/40 hover:bg-white/5 hover:border-white/10"
                        )}
                      >
                        <effect.icon className={cn(
                          "w-6 h-6",
                          activeEffect === effect.name ? "text-cyan-400 animate-pulse" : "group-hover:text-white"
                        )} />
                        <span className="text-[10px] font-bold uppercase tracking-wider text-center">{effect.name}</span>
                      </button>
                    ))}
                  </div>
                </Card>

                <Card className="flex-1 bg-black/40 min-h-[300px] flex items-center justify-center border-dashed border-white/10">
                   <div className="text-center">
                    <div className="relative mb-6">
                       <Keyboard className="w-20 h-20 text-white/5 mx-auto" />
                       <div className="absolute inset-0 flex items-center justify-center">
                          <motion.div 
                            animate={{ 
                              scale: [1, 1.2, 1],
                              opacity: [0.5, 1, 0.5]
                            }}
                            transition={{ repeat: Infinity, duration: 3 }}
                            className="w-12 h-12 rounded-full blur-2xl"
                            style={{ backgroundColor: selectedColor }}
                          />
                       </div>
                    </div>
                    <p className="text-white font-medium mb-1">Live Preview</p>
                    <p className="text-white/40 text-xs">Preview your custom lighting setup</p>
                   </div>
                </Card>
              </div>
            </div>
          </motion.div>
        );

      case 'mapping':
        return (
          <motion.div 
            key="mapping"
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            className="space-y-6"
          >
            <SectionTitle subtitle="Assign keys and secondary functions">Key Mapping</SectionTitle>
            
            <Card className="p-1 overflow-x-auto">
              <div className="min-w-[800px] p-6">
                <KeyboardLayout onKeyClick={(label) => console.log('Key clicked:', label)} configs={keyConfigs} />
              </div>
            </Card>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <Card>
                <div className="flex items-center gap-3 mb-6">
                  <div className="w-8 h-8 rounded-lg bg-white/5 flex items-center justify-center">
                    <Command className="w-4 h-4 text-white/60" />
                  </div>
                  <div>
                    <h3 className="text-sm font-semibold text-white">Layer Configuration</h3>
                    <p className="text-[10px] text-white/40 uppercase tracking-wider">Modifier State</p>
                  </div>
                </div>
                
                <div className="space-y-2">
                  {['Default', 'Function Layer (FN)', 'Gaming Layer', 'Productivity'].map((layer, i) => (
                    <button 
                      key={layer}
                      className={cn(
                        "w-full flex items-center justify-between p-3 rounded-xl border text-sm transition-all",
                        i === 0 
                          ? "bg-white/10 border-white/20 text-white" 
                          : "bg-white/[0.02] border-white/5 text-white/40 hover:bg-white/5"
                      )}
                    >
                      <span className="font-medium tracking-tight">{layer}</span>
                      {i === 0 && <CheckCircle2 className="w-4 h-4 text-emerald-400" />}
                    </button>
                  ))}
                </div>
              </Card>

              <Card>
                <div className="flex items-center gap-3 mb-6">
                  <div className="w-8 h-8 rounded-lg bg-white/5 flex items-center justify-center">
                    <RotateCcw className="w-4 h-4 text-white/60" />
                  </div>
                  <div>
                    <h3 className="text-sm font-semibold text-white">Reset & Default</h3>
                    <p className="text-[10px] text-white/40 uppercase tracking-wider">Dangerous Actions</p>
                  </div>
                </div>
                <div className="space-y-3">
                  <p className="text-xs text-white/30 mb-4 leading-relaxed">
                    Restore all key mappings to their factory defaults. This action cannot be undone and will overwrite all custom profiles.
                  </p>
                  <button className="w-full py-3 rounded-xl border border-red-500/30 text-red-400 text-xs font-bold uppercase tracking-widest hover:bg-red-500/10 transition-colors">
                    Reset All Mappings
                  </button>
                </div>
              </Card>
            </div>
          </motion.div>
        );

      case 'macros':
        return (
          <motion.div 
            key="macros"
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            className="space-y-6"
          >
            <SectionTitle subtitle="Record and manage complex key sequences">Macro Engine</SectionTitle>
            <Card className="p-20 flex flex-col items-center justify-center border-dashed border-white/10 opacity-60">
               <Zap className="w-16 h-16 text-white/10 mb-6" />
               <p className="text-white font-medium text-lg mb-2">No Macros Recorded</p>
               <p className="text-white/40 text-sm mb-8">Start recording to create complex shortcuts</p>
               <button className="px-8 py-3 bg-white text-black text-xs font-bold uppercase tracking-widest rounded-xl hover:bg-white/90 transition-all active:scale-95 shadow-lg shadow-white/10">
                 New Macro
               </button>
            </Card>
          </motion.div>
        );

      case 'settings':
        return (
          <motion.div 
            key="settings"
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            className="space-y-6"
          >
            <SectionTitle subtitle="Device behavior and application preferences">System Settings</SectionTitle>
            
            <Card className="divide-y divide-white/5 space-y-4">
              <div className="flex items-center justify-between pb-4">
                <div>
                  <p className="text-white font-medium text-sm">Launch on Startup</p>
                  <p className="text-white/40 text-xs">Automatically start KeyCraft Pro when you log in</p>
                </div>
                <div className="w-12 h-6 bg-emerald-500 rounded-full relative p-1 cursor-pointer">
                  <div className="w-4 h-4 bg-white rounded-full absolute right-1" />
                </div>
              </div>

              <div className="flex items-center justify-between py-4">
                <div>
                  <p className="text-white font-medium text-sm">Low Battery Alerts</p>
                  <p className="text-white/40 text-xs">Notify when battery level is below 15%</p>
                </div>
                <div className="w-12 h-6 bg-emerald-500 rounded-full relative p-1 cursor-pointer">
                  <div className="w-4 h-4 bg-white rounded-full absolute right-1" />
                </div>
              </div>

              <div className="flex items-center justify-between py-4">
                <div>
                  <p className="text-white font-medium text-sm">Hardware Acceleration</p>
                  <p className="text-white/40 text-xs">Use GPU for advanced RGB rendering effects</p>
                </div>
                <div className="w-12 h-6 bg-white/10 rounded-full relative p-1 cursor-pointer">
                  <div className="w-4 h-4 bg-white/40 rounded-full absolute left-1" />
                </div>
              </div>

               <div className="flex items-center justify-between py-4">
                <div>
                  <p className="text-white font-medium text-sm">Poll Rate Optimization</p>
                  <p className="text-white/40 text-xs">Dynamically adjust polling based on active application</p>
                </div>
                <div className="w-12 h-6 bg-emerald-500 rounded-full relative p-1 cursor-pointer">
                  <div className="w-4 h-4 bg-white rounded-full absolute right-1" />
                </div>
              </div>
            </Card>

            <Card className="bg-red-500/5 border-red-500/20">
               <div className="flex items-center gap-4">
                  <AlertCircle className="w-6 h-6 text-red-400" />
                  <div className="flex-1">
                    <p className="text-red-400 font-semibold text-sm">Factory Reset</p>
                    <p className="text-red-400/60 text-xs">Wipe all local data and configurations from the device</p>
                  </div>
                  <button className="px-6 py-2 bg-red-500 text-white text-[10px] font-bold uppercase tracking-widest rounded-lg hover:bg-red-600 transition-colors shadow-lg shadow-red-500/20">
                    Purge Data
                  </button>
               </div>
            </Card>
          </motion.div>
        );
    }
  };

  return (
    <div className="min-h-screen bg-[#09090b] text-white flex overflow-hidden selection:bg-cyan-500/30">
      {/* --- Sidebar --- */}
      <aside className="w-64 border-r border-white/5 bg-black/40 backdrop-blur-3xl flex flex-col p-6 z-20">
        <div className="flex items-center gap-3 mb-12 px-2">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-cyan-500 to-purple-600 p-[1px]">
            <div className="w-full h-full bg-[#09090b] rounded-[11px] flex items-center justify-center">
               <Keyboard className="w-6 h-6 text-white" />
            </div>
          </div>
          <h1 className="text-xl font-bold tracking-tighter">KeyCraft<span className="text-cyan-400">Pro</span></h1>
        </div>

        <div className="space-y-2 flex-1">
          <SidebarItem icon={Cpu} label="System" active={activeView === 'dashboard'} onClick={() => setActiveView('dashboard')} />
          <SidebarItem icon={Palette} label="Lighting" active={activeView === 'lighting'} onClick={() => setActiveView('lighting')} />
          <SidebarItem icon={Layers} label="Mapping" active={activeView === 'mapping'} onClick={() => setActiveView('mapping')} />
          <SidebarItem icon={Zap} label="Macros" active={activeView === 'macros'} onClick={() => setActiveView('macros')} />
          <SidebarItem icon={Settings} label="Settings" active={activeView === 'settings'} onClick={() => setActiveView('settings')} />
        </div>

        <div className="mt-auto">
          <Card className="p-4 bg-white/5 border-white/10 rounded-xl">
             <div className="flex items-center gap-2 mb-2">
               <div className="w-1.5 h-1.5 rounded-full bg-emerald-500 animate-pulse" />
               <span className="text-[10px] font-bold uppercase tracking-widest text-emerald-400">Connected</span>
             </div>
             <p className="text-[10px] text-white/40 uppercase tracking-wider mb-1">Device Model</p>
             <p className="text-xs font-semibold tracking-tight text-white mb-3">KeyCraft Ultra TKL</p>
          </Card>
        </div>
      </aside>

      {/* --- Main View --- */}
      <main className="flex-1 overflow-y-auto relative bg-[radial-gradient(circle_at_50%_0%,rgba(0,242,255,0.03)_0%,transparent_50%)]">
        {/* Top Header */}
        <header className="sticky top-0 h-16 border-b border-white/5 bg-black/40 backdrop-blur-md flex items-center justify-between px-8 z-10">
          <div className="flex items-center gap-2">
            <span className="text-xs font-medium text-white/40 tracking-wider uppercase">Active View /</span>
            <span className="text-xs font-bold text-white tracking-widest uppercase">{activeView}</span>
          </div>

          <div className="flex items-center gap-6">
            <div className="flex items-center gap-4 pr-6 border-r border-white/10">
               <div className="flex flex-col items-end">
                  <p className="text-[9px] font-bold text-white/40 uppercase tracking-widest leading-none mb-1 text-right">System Profile</p>
                  <p className="text-xs font-semibold text-white leading-none text-right">{activeProfile}</p>
               </div>
               <button className="w-8 h-8 rounded-lg bg-white/5 hover:bg-white/10 flex items-center justify-center transition-colors">
                  <Sliders className="w-4 h-4 text-white" />
               </button>
            </div>

            <button 
              onClick={handleSave}
              disabled={isSaving}
              className={cn(
                "flex items-center gap-2 px-5 py-2 rounded-lg text-xs font-bold uppercase tracking-widest transition-all",
                isSaving 
                  ? "bg-emerald-500 text-white" 
                  : "bg-white text-black hover:bg-white/90 active:scale-95 shadow-lg shadow-white/5"
              )}
            >
              {isSaving ? (
                <>
                  <CheckCircle2 className="w-4 h-4 animate-bounce" />
                  Synced
                </>
              ) : (
                <>
                  <Save className="w-4 h-4" />
                  Apply Changes
                </>
              )}
            </button>
          </div>
        </header>

        {/* Content Area */}
        <div className="p-10 max-w-6xl mx-auto">
          <AnimatePresence mode="wait">
            {renderContent()}
          </AnimatePresence>
        </div>
      </main>

      {/* --- Background Glows --- */}
      <div className="fixed top-0 right-0 w-[500px] h-[500px] bg-cyan-500/5 blur-[150px] -z-10 rounded-full" />
      <div className="fixed bottom-0 left-0 w-[500px] h-[500px] bg-purple-600/5 blur-[150px] -z-10 rounded-full" />
    </div>
  );
}
