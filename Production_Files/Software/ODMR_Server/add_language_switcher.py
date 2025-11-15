#!/usr/bin/env python3
"""
Add language switcher to all HTML pages
"""

import re
from pathlib import Path

# Translation dictionary for all pages
TRANSLATIONS = {
    'messung.html': {
        'de': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Messung (on Device)',
            'nav_measurement_webserial': 'Messung (WebSerial)',
            'nav_alignment': 'Justage',
            'nav_info': 'Weitere Infos',
            'title': 'NV Experimente - Messung durchführen',
            'page_title': 'Messung der Lichtintensität in Abhängigkeit von der Frequenz',
            'scan_params': 'Scan-Parameter einstellen',
            'start_freq': 'Startfrequenz (MHz)',
            'end_freq': 'Endfrequenz (MHz)',
            'step_size': 'Schrittweite (MHz)',
            'apply_data': 'Daten übernehmen',
            'reset': 'Zurücksetzen',
            'download_csv': 'Download CSV',
            'clear': 'Leeren',
            'start_measurement': 'Messung starten',
            'measurement_running': 'Messung läuft',
            'single_measurement': 'Einzelmessung',
            'continuous_measurement': 'Dauermessung',
        },
        'en': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Measurement (on Device)',
            'nav_measurement_webserial': 'Measurement (WebSerial)',
            'nav_alignment': 'Alignment',
            'nav_info': 'More Info',
            'title': 'NV Experiments - Perform Measurement',
            'page_title': 'Measurement of Light Intensity as a Function of Frequency',
            'scan_params': 'Set Scan Parameters',
            'start_freq': 'Start Frequency (MHz)',
            'end_freq': 'End Frequency (MHz)',
            'step_size': 'Step Size (MHz)',
            'apply_data': 'Apply Data',
            'reset': 'Reset',
            'download_csv': 'Download CSV',
            'clear': 'Clear',
            'start_measurement': 'Start Measurement',
            'measurement_running': 'Measurement Running',
            'single_measurement': 'Single Measurement',
            'continuous_measurement': 'Continuous Measurement',
        }
    },
    'messung_webserial.html': {
        'de': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Messung (on Device)',
            'nav_measurement_webserial': 'Messung (WebSerial)',
            'nav_alignment': 'Justage',
            'nav_info': 'Weitere Infos',
            'title': 'NV Experimente - Messung durchführen',
            'page_title': 'Messung der Lichtintensität in Abhängigkeit von der Frequenz',
            'freq_sweep': 'Frequenz-Sweep',
            'live_monitor': 'Live Monitor',
            'connect_serial': 'Serial verbinden',
            'start_measurement': 'Messung starten',
            'stop_measurement': 'Messung stoppen',
            'serial_status': 'Serial Status:',
            'not_connected': 'Nicht verbunden',
        },
        'en': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Measurement (on Device)',
            'nav_measurement_webserial': 'Measurement (WebSerial)',
            'nav_alignment': 'Alignment',
            'nav_info': 'More Info',
            'title': 'NV Experiments - Perform Measurement',
            'page_title': 'Measurement of Light Intensity as a Function of Frequency',
            'freq_sweep': 'Frequency Sweep',
            'live_monitor': 'Live Monitor',
            'connect_serial': 'Connect Serial',
            'start_measurement': 'Start Measurement',
            'stop_measurement': 'Stop Measurement',
            'serial_status': 'Serial Status:',
            'not_connected': 'Not connected',
        }
    },
    'justage.html': {
        'de': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Messung (on Device)',
            'nav_measurement_webserial': 'Messung (WebSerial)',
            'nav_alignment': 'Justage',
            'nav_info': 'Weitere Infos',
            'title': 'NV Experimente - Justage',
            'page_title': 'Photodioden-Justage',
            'intensity_monitor': 'Intensitäts-Monitor',
            'start_monitoring': 'Monitoring starten',
            'stop_monitoring': 'Monitoring stoppen',
            'current_intensity': 'Aktuelle Intensität',
        },
        'en': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Measurement (on Device)',
            'nav_measurement_webserial': 'Measurement (WebSerial)',
            'nav_alignment': 'Alignment',
            'nav_info': 'More Info',
            'title': 'NV Experiments - Alignment',
            'page_title': 'Photodiode Alignment',
            'intensity_monitor': 'Intensity Monitor',
            'start_monitoring': 'Start Monitoring',
            'stop_monitoring': 'Stop Monitoring',
            'current_intensity': 'Current Intensity',
        }
    },
    'infos.html': {
        'de': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Messung (on Device)',
            'nav_measurement_webserial': 'Messung (WebSerial)',
            'nav_alignment': 'Justage',
            'nav_info': 'Weitere Infos',
            'title': 'NV Experimente - Weitere Informationen',
            'page_title': 'Weitere Informationen',
            'about_experiment': 'Über das Experiment',
            'technical_details': 'Technische Details',
        },
        'en': {
            'nav_start': 'Start',
            'nav_measurement_device': 'Measurement (on Device)',
            'nav_measurement_webserial': 'Measurement (WebSerial)',
            'nav_alignment': 'Alignment',
            'nav_info': 'More Info',
            'title': 'NV Experiments - More Information',
            'page_title': 'More Information',
            'about_experiment': 'About the Experiment',
            'technical_details': 'Technical Details',
        }
    }
}

def add_language_switcher_to_navbar(html_content, filename):
    """Add language switcher dropdown to navbar"""
    
    # Language switcher HTML
    lang_switcher = '''          <li class="nav-item dropdown">
            <a class="nav-link dropdown-toggle" href="#" id="langDropdown" role="button" data-bs-toggle="dropdown" aria-expanded="false">
              🌐 DE
            </a>
            <ul class="dropdown-menu" aria-labelledby="langDropdown">
              <li><a class="dropdown-item" href="#" onclick="setLanguage('de'); return false;">🇩🇪 Deutsch</a></li>
              <li><a class="dropdown-item" href="#" onclick="setLanguage('en'); return false;">🇺🇸 English</a></li>
            </ul>
          </li>'''
    
    # Find the closing </ul> before </div> in navbar
    pattern = r'(</ul>\s*</div>\s*</div>\s*</nav>)'
    replacement = lang_switcher + '\n        \\1'
    
    html_content = re.sub(pattern, replacement, html_content, count=1)
    
    return html_content

def add_translation_script(html_content, filename):
    """Add translation script before closing body tag"""
    
    if filename not in TRANSLATIONS:
        return html_content
    
    translations_de = TRANSLATIONS[filename]['de']
    translations_en = TRANSLATIONS[filename]['en']
    
    # Create JavaScript object
    trans_de_js = ',\n        '.join([f'"{k}": "{v}"' for k, v in translations_de.items()])
    trans_en_js = ',\n        '.join([f'"{k}": "{v}"' for k, v in translations_en.items()])
    
    translation_script = f'''
  <script>
    // Multi-language support
    const translations = {{
      de: {{
        {trans_de_js}
      }},
      en: {{
        {trans_en_js}
      }}
    }};
    
    function setLanguage(lang) {{
      localStorage.setItem('language', lang);
      updateLanguage(lang);
      
      // Update dropdown display
      const langDropdown = document.getElementById('langDropdown');
      if (langDropdown) {{
        langDropdown.innerHTML = `🌐 ${{lang.toUpperCase()}}`;
      }}
    }}
    
    function updateLanguage(lang) {{
      const elements = document.querySelectorAll('[data-lang-key]');
      elements.forEach(element => {{
        const key = element.getAttribute('data-lang-key');
        if (translations[lang] && translations[lang][key]) {{
          element.textContent = translations[lang][key];
        }}
      }});
      
      // Update title
      if (translations[lang] && translations[lang].title) {{
        document.title = translations[lang].title;
      }}
    }}
    
    // Initialize language on page load
    document.addEventListener('DOMContentLoaded', function() {{
      const savedLang = localStorage.getItem('language') || 'de';
      setLanguage(savedLang);
    }});
  </script>
'''
    
    # Insert before closing body tag
    html_content = html_content.replace('</body>', translation_script + '</body>')
    
    return html_content

def add_data_lang_keys_to_navbar(html_content):
    """Add data-lang-key attributes to navbar items"""
    
    replacements = [
        (r'<a class="nav-link[^"]*" href="index\.html">Start</a>',
         '<a class="nav-link" href="index.html" data-lang-key="nav_start">Start</a>'),
        (r'<a class="nav-link[^"]*" href="messung\.html">Messung \(on Device\)</a>',
         '<a class="nav-link" href="messung.html" data-lang-key="nav_measurement_device">Messung (on Device)</a>'),
        (r'<a class="nav-link[^"]*" href="messung_webserial\.html">Messung \(WebSerial\)</a>',
         '<a class="nav-link" href="messung_webserial.html" data-lang-key="nav_measurement_webserial">Messung (WebSerial)</a>'),
        (r'<a class="nav-link[^"]*" href="justage\.html">Justage[^<]*</a>',
         '<a class="nav-link" href="justage.html" data-lang-key="nav_alignment">Justage</a>'),
        (r'<a class="nav-link[^"]*" href="infos\.html">Weitere Infos</a>',
         '<a class="nav-link" href="infos.html" data-lang-key="nav_info">Weitere Infos</a>'),
    ]
    
    for pattern, replacement in replacements:
        html_content = re.sub(pattern, replacement, html_content)
    
    return html_content

def process_html_file(filepath):
    """Process a single HTML file"""
    filename = filepath.name
    
    if filename == 'index.html':
        print(f"Skipping {filename} (already has language switcher)")
        return False
    
    print(f"Processing {filename}...")
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check if already has language switcher
    if 'langDropdown' in content:
        print(f"  Skipping {filename} (already has language switcher)")
        return False
    
    # Add data-lang-key to navbar
    content = add_data_lang_keys_to_navbar(content)
    
    # Add language switcher to navbar
    content = add_language_switcher_to_navbar(content, filename)
    
    # Add translation script
    content = add_translation_script(content, filename)
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"  ✓ Added language switcher to {filename}")
    return True

def main():
    script_dir = Path(__file__).parent
    html_dir = script_dir / "src" / "website_html"
    
    html_files = list(html_dir.glob("*.html"))
    
    modified_count = 0
    for html_file in html_files:
        if process_html_file(html_file):
            modified_count += 1
    
    print(f"\n✓ Language switcher added to {modified_count} files")

if __name__ == "__main__":
    main()
