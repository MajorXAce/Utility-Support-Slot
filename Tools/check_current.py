"""Current offline own-fixture checks using immutable reviewed SDK inputs."""
import json,os,subprocess,sys,shutil,hashlib,argparse
from sdk_inputs import ROOT,sha,validate_inputs

def receipt_filename(names,msvc=False):
    prefix='checks-msvc' if msvc else 'checks'
    filename=prefix+('' if names is None else '-'+'-'.join(names))+'.json'
    if len(filename)>180:
        digest=hashlib.sha256(json.dumps(names,separators=(',',':')).encode('utf-8')).hexdigest()[:12]
        filename=f'{prefix}-subset-{len(names)}-{digest}.json'
    return filename

def main(names=None,msvc=False,evidence_name='current-checks'):
    baseline,sdk,compiler=validate_inputs()
    build=ROOT/('build/current-checks-msvc' if msvc else 'build/current-checks');build.mkdir(parents=True,exist_ok=True)
    evidence=ROOT/'build/checks'/evidence_name;evidence.mkdir(parents=True,exist_ok=True)
    # Platform fixtures need their parent even on an MSVC-first clean checkout.
    (ROOT/'build/hf-uss-039').mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    for kind in ('GLOBAL','LOCAL'):
        env[f'ZIG_{kind}_CACHE_DIR']=str(ROOT/'build/hf-uss-039'/f'zig-{kind.lower()}-cache')
    if not msvc and subprocess.check_output([str(compiler),'version'],env=env,text=True).strip()!=baseline['compiler_version']:
        raise RuntimeError('Wrong compiler version')
    # The replacement probe aggregate executes command/adapter/evidence/coordinator regressions.
    cases=names or ['uss_normal_startup','uss_menu_startup_inputs','uss_native_menu_session','uss_native_menu_lifetime','uss_native_menu_frame','uss_native_menu_controls','uss_menu_settings_controller','uss_dedicated_input_windows','uss_menu_capture_guard','uss_dedicated_capture','uss_native_binding_candidate','uss_binding_reservation_files','uss_native_binding_context','uss_binding_reservations','uss_native_binding_host','uss_native_binding_reader','uss_native_menu_lifecycle','uss_native_menu_ownership','uss_native_menu_write','uss_native_menu_chrome','uss_native_menu_rows','uss_native_assignment_display','uss_hud_drop','uss_hud_presentation','uss_hud_runtime','uss_solo','uss_session','uss_recovery','uss_gameplay_application','uss_player_control','uss_automatic_retained','uss_automatic_replacement','uss_active_replacement','uss_automatic_controller','uss_gameplay_loop','uss_refill_driver','uss_refill_entries','uss_supply_reader','uss_refill_core','uss_refill_target','uss_supply_observer','uss_refill_amount','uss_refill_components','uss_pickup_readers','uss_pickup_completion','uss_refill_source_policy','uss_native_menu_sdk','uss_hud_windows','uss_hud_ack','uss_refill_transaction','uss_native_menu_observation','uss_native_menu_entries','uss_native_menu_host','uss_native_menu_data','uss_binding_capture','uss_vpm_windows','uss_vpm_menus','uss_menu_handover','uss_settings','uss_settings_store','sdk_compat','probe','formation_probe','replacement_probe','core_lifecycle','core_observation','custom_support','uss_zone_hygiene','uss_ping_filter','uss_ping_filter_native','uss_ping_desired','pair_owner','pair_transaction','pair_member','support_observation','primary_readiness','capture_probe_inactive','capture_probe_enabled']
    results=[]
    for name in cases:
        source=ROOT/'Tests'/('capture_probe_harness.c' if name.startswith('capture_probe_') else name+'_harness.c')
        output=build/(name+'.exe')
        cmd=[str(compiler),'cc','-std=c11','-target','x86_64-windows-gnu','-O2','-UNDEBUG','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I',str(sdk),str(source),'-o',str(output)]
        if name.startswith('capture_probe_') or name in ('formation_probe','replacement_probe','uss_normal_startup'):
            cmd+=['-lbcrypt']
            if name.endswith('_enabled'):cmd+=['-DHF_USS_CAPTURE_ENABLED=1']
        if name in ('uss_player_control','uss_gameplay_application','uss_session','uss_normal_startup'):cmd+=['-luser32']
        if msvc:
            cl=shutil.which('cl.exe')
            if not cl:raise RuntimeError('Run Tools/check_current.cmd for MSVC environment')
            cmd=[cl,'/nologo','/experimental:c11atomics','/std:c11','/W4','/WX','/TC','/UNDEBUG','/I',str(sdk),str(source),'/Fo'+str(build/(name+'.obj')),'/Fe'+str(output)]
            if name=='capture_probe_enabled':cmd+=['/DHF_USS_CAPTURE_ENABLED=1']
            if name.startswith('capture_probe_') or name in ('formation_probe','replacement_probe','uss_normal_startup'):cmd+=['/link','bcrypt.lib','kernel32.lib']
        if msvc and name in ('uss_player_control','uss_gameplay_application','uss_session','uss_normal_startup'):
            cmd+=['user32.lib'] if '/link' in cmd else ['/link','user32.lib']
        c=subprocess.run(cmd,cwd=ROOT,env=env,text=True,capture_output=True)
        (evidence/(name+'-compile.txt')).write_text(c.stdout+c.stderr)
        if c.returncode:raise RuntimeError(c.stdout+c.stderr)
        t=subprocess.run([str(output)],cwd=ROOT,text=True,capture_output=True)
        (evidence/(name+'-result.txt')).write_text(t.stdout+t.stderr)
        if t.returncode:raise RuntimeError(t.stdout+t.stderr)
        print(name+': PASS',flush=True)
        results.append({'name':name,'command':cmd,'source_sha256':sha(source),'executable_sha256':sha(output)})
    inputs={str(p.relative_to(ROOT)).replace('\\','/'):sha(p) for folder in ('src','ResearchSource','Tests','Tools') for p in (ROOT/folder).iterdir() if p.suffix in ('.h','.c','.py','.json','.cmd')}
    receipt={'baseline':baseline,'cases':results,'inputs':inputs,'game_access':False,'package_built':False,'deployed':False,'compiler_kind':'MSVC' if msvc else 'Zig'}
    # A selected subset does not overwrite the full-run receipt.
    filename=receipt_filename(names,msvc)
    (evidence/filename).write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt
BATCHES={
    'menu':['uss_normal_startup','uss_menu_startup_inputs','uss_native_menu_session','uss_native_menu_lifetime','uss_native_menu_frame','uss_native_menu_controls','uss_menu_settings_controller','uss_dedicated_input_windows','uss_menu_capture_guard','uss_dedicated_capture','uss_native_binding_candidate','uss_binding_reservation_files','uss_native_binding_context','uss_binding_reservations','uss_native_binding_host','uss_native_binding_reader','uss_gameplay_loop','uss_gameplay_application','uss_native_menu_lifecycle','uss_native_menu_ownership','uss_native_menu_write','uss_native_menu_chrome','uss_native_menu_rows','uss_native_assignment_display','uss_native_menu_sdk','uss_native_menu_observation','uss_native_menu_entries','uss_native_menu_host','uss_native_menu_data','uss_binding_capture','uss_vpm_windows','uss_vpm_menus','uss_menu_handover','uss_settings','uss_settings_store'],
    'hud':['uss_refill_transaction','uss_refill_driver','uss_gameplay_loop','uss_hud_drop','uss_hud_presentation','uss_hud_runtime','uss_hud_ack','uss_hud_windows','uss_gameplay_application','uss_player_control','uss_recovery','uss_solo','uss_automatic_retained','uss_automatic_replacement','pair_transaction','uss_session','formation_probe','replacement_probe'],
    'solo':['uss_solo','uss_automatic_retained','uss_automatic_replacement','uss_recovery','core_observation','uss_session'],
    'session':['uss_session','pair_owner','core_observation','core_lifecycle','uss_gameplay_loop','uss_recovery','uss_gameplay_application','uss_automatic_retained','uss_automatic_replacement','formation_probe','replacement_probe'],
    'recovery':['uss_recovery','uss_gameplay_application','uss_player_control','uss_gameplay_loop','uss_automatic_retained','uss_automatic_replacement','formation_probe','replacement_probe'],
    'player':['uss_gameplay_application','uss_player_control','uss_settings','uss_gameplay_loop','uss_automatic_controller','uss_automatic_retained','uss_automatic_replacement','formation_probe'],
    'automatic':['uss_automatic_retained','uss_automatic_replacement','uss_automatic_controller','uss_active_replacement','uss_gameplay_loop','uss_refill_core','uss_refill_target','formation_probe','replacement_probe'],
}
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--msvc',action='store_true')
    parser.add_argument('--batch',choices=sorted(BATCHES))
    parser.add_argument('--evidence',default='current-checks')
    args=parser.parse_args()
    main(names=BATCHES[args.batch] if args.batch else None,msvc=args.msvc,evidence_name=args.evidence)
