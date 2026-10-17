#!/usr/bin/env python3
"""Import the study's actual IBM Carbon 16-unit artwork, without a runtime dependency.

Run only when updating the checked-in artwork from an existing local Carbon package.
The native bundle uses the pre-rendered atlas and the generated C enum.
"""
from pathlib import Path
import json,re,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'design/prototype/node_modules/@carbon/icons-react'
OUT=ROOT/'assets/ui/carbon'
NAMES='Add Close Search ChevronDown ChevronUp ChevronLeft ChevronRight Table ChartBar ChartLine Copy Paste Cut Undo Redo TextBold TextItalic TextUnderline TextAlignLeft TextAlignCenter TextAlignRight TextWrap CurrencyDollar Percentage Filter FilterRemove SortAscending SortDescending SettingsAdjust OverflowMenuHorizontal OverflowMenuVertical Pin PinFilled Maximize Minimize DataStructured DataView Calendar StringText Tag Function CheckmarkOutline Wallet Sql Code Checkmark ArrowsHorizontal View Launch Save Renew FolderOpen ArrowDown ArrowUp ArrowLeft ArrowRight TextColor ColorPalette Grid Information WarningAlt Help Keyboard List Column Row ChartColumn Draggable TrashCan Menu DocumentBlank ZoomIn ZoomOut Document'.split()
OUT.mkdir(parents=True,exist_ok=True)
for name in NAMES:
    source=(SOURCE/'es'/f'{name}.js').read_text()
    if 'if (size === 16' in source:
        start=source.index('if (size === 16')
        ends=[source.find(marker,start+1) for marker in ('\n\treturn React.createElement(Icon','\n\tif (size ===')]
        end=min(value for value in ends if value>=0)
        part=source[start:end]
    else:
        start=source.index('\n\treturn React.createElement(Icon')
        part=source[start:source.index('\n});',start)]
    viewbox=re.search(r'viewBox: "([^"]+)"',part).group(1)
    svg=ET.Element('svg',xmlns='http://www.w3.org/2000/svg',viewBox=viewbox,width='16',height='16',fill='white')
    for tag,attrs in re.findall(r'jsx\("(path|circle|rect|polygon|line|polyline)", \{([^}]+)\}',part):
        data={}
        for match in re.finditer(r'(?:"([\w-]+)"|(\w+)):\s*("[^"]*"|[-.\d]+)',attrs):
            key=match.group(1) or match.group(2)
            if key.startswith('data-') or key=='key': continue
            key={'fillRule':'fill-rule','clipRule':'clip-rule','strokeWidth':'stroke-width'}.get(key,key)
            value=json.loads(match.group(3)) if match.group(3).startswith('"') else match.group(3)
            data[key]=str(value)
        ET.SubElement(svg,tag,data)
    if not len(svg): raise ValueError(f'{name}: no geometry')
    xml=ET.tostring(svg,encoding='unicode')
    (OUT/f'{name}.svg').write_text('<!-- IBM Carbon icons. Copyright IBM Corp. 2016, 2023. Apache-2.0. Extracted 16-unit artwork; white fill for native tinting. -->\n'+xml+'\n')
(ROOT/'assets/ui/CARBON-LICENSE.txt').write_text((SOURCE/'LICENSE').read_text())
(ROOT/'assets/ui/carbon-icons.json').write_text(json.dumps({'source':'IBM Carbon @carbon/icons-react, 16-unit icons from the design study','license':'Apache-2.0','cell':64,'columns':8,'names':NAMES},indent=2)+'\n')
header=['/* Generated from the actual IBM Carbon artwork used by the UI specification. */','#ifndef DX_CARBON_ICONS_H','#define DX_CARBON_ICONS_H','typedef enum {']
for i,name in enumerate(NAMES): header.append(f'    CARBON_{re.sub(r"(?<!^)(?=[A-Z])","_",name).upper()} = {i},')
header+=['    CARBON_COUNT', '} UICarbonIcon;', '#endif','']
(ROOT/'src/ui/carbon_icons.h').write_text('\n'.join(header))
print(f'Imported {len(NAMES)} original Carbon icons')
