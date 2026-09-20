#!/usr/bin/env python3
"""Build the playable 30-city prototype from rom_inspect JSON. No ROM modifications.
Positions come from ROM tables; routes are verified against the original road screen.
"""
import argparse
import json
from pathlib import Path

OWNERS = {4:1,2:2,0:3,1:4,3:5,5:6}

def build(data):
    if len(data['cities']) != 30 or len(data['officers']) != 241:
        raise ValueError('Expected verified 30-city / 241-officer inventory')
    result = {
        'scenario_id':'zhongyuan-original-map-v2', 'max_owner':6,
        'opening':'刘备军据守新野、荆州、衡阳。统筹三城，逐步争夺天下三十城。',
        'provenance': {
            'rom_sha256':'9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959',
            'initial_data':'ROM city records at 0x271c, officer records at 0x2b54; before opening AI turns.',
            'owners':'flags & 7, confirmed in CPU A49B-A4A6; remapped to six desktop faction IDs.',
            'topology':'Coordinates decoded from ROM 0x1de75/0x1de93; 64 roads decoded from ROM pointer table 0x19a50 and independently matched to original 线路 screen pixel components.',
            'derived':'troops = resident infantry+cavalry+archers; ability = strongest resident martial; development = max(1,land//10); grain = 300+troops//10. These are prototype adaptations.',
        }, 'cities':[]
    }
    for c in data['cities']:
        officers = [data['officers'][i] for i in c['officer_slots'] if i is not None]
        strongest = max(officers, key=lambda o:o['martial'])
        troops = sum(o['infantry']+o['cavalry']+o['archers'] for o in officers)
        i = c['id']
        reference = {k:c[k] for k in ('rom_file_offset','flags_raw','faction_id_candidate','land','commerce','population','control','officer_slots','gold')}
        reference['officers'] = [{k:o[k] for k in ('id','name','stamina','intelligence','martial','virtue','loyalty','infantry','cavalry','archers')} for o in officers]
        result['cities'].append({
            'id':i,'name':c['name'],'owner':OWNERS[c['faction_id_candidate']],
            'gold':c['gold'],'grain':300+troops//10,'troops':troops,
            'development':max(1,c['land']//10),
            'general':strongest['name'] or ('武将 #%d' % strongest['id']),
            'ability':strongest['martial'], 'reference':reference,
            'neighbors':sorted(b if a==i else a for a,b in data['world_map']['edges'] if a==i or b==i),
            'pos':[data['world_map']['positions'][i][0]/256,data['world_map']['positions'][i][1]/160],
        })
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    args.output.write_text(json.dumps(build(json.loads(args.inventory.read_text())),ensure_ascii=False,indent=2)+'\n')
