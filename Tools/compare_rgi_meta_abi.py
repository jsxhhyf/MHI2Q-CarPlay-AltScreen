#!/usr/bin/env python3
"""Compare a clang-built libcarplay_rgi_meta.so against the SDP-built reference.

Code differs between GCC 8.5 and clang; what must not differ is the ABI the
module presents to dio_manager and consumes from QNX:

  * dynamic exports (exact), DT_NEEDED, DT_SONAME, symbol version needs
  * imports: every clang import must be one the reference imports, or listed
    in ALLOWED_EXTRA with a reason
  * ELF header flags and ARM build attributes that affect the call ABI
  * the size of every data object, matched by name across both .symtab
    tables.  The large globals embed every pthread/sigaction/stat object the
    module owns, so a wrong shim layout shows up here as a size mismatch.

Exit status 0 only when nothing is unexplained.
Usage: compare_rgi_meta_abi.py REFERENCE.so CANDIDATE.so
"""
import re
import sys
from collections import Counter

from elftools.elf.elffile import ELFFile

# Imports the reference does not have, accepted with the reason.  Empty on
# purpose: anything clang adds (bcmp, __aeabi_idiv...) must be built in.
ALLOWED_EXTRA = {
}
# Reference imports whose only callers are dead in the reference itself (zero
# call sites; SDP ld keeps hidden globals, lld --gc-sections drops them).
DEAD_IN_REFERENCE = {
    '_exit': 'spa_signal_guard_chain, 0 callers',
    'raise': 'spa_signal_guard_chain, 0 callers',
    'pthread_rwlock_wrlock': 'bus_on/bus_off, 0 callers',
    'strdup': 'log_dump_file_once, 0 callers',
    '__aeabi_uidivmod': 'only libgcc __divdi3/__udivdi3',
}
# File-local statics that clang GlobalOpt reshapes.  Never externally visible.
#   split: struct scalar-replaced into fields; candidate sizes must sum to ref.
#   bool:  0/1 int flag (mutex-guarded, address never taken) shrunk to 1 byte.
GLOBALOPT = {
    'g_coverart': 'split',
    'runtime_initialized': 'bool', 'runtime_shutting_down': 'bool',
    'seam_confirmed': 'bool', 'worker_exited': 'bool', 'worker_shutdown': 'bool',
    'worker_started': 'bool', 'worker_starting': 'bool',
}
# Deliberate source changes relative to the reference (tolerated, not required,
# so an unmodified Fix4 source still compares clean): name -> (ref, new, why).
INTENDED = {
    'rgd_msg_filter': (6, 8, '0x1D03 IdentificationRejected listed for logging (ER RGI)'),
}
# The two state blocks that embed every lock/cond/once the module owns.
KEY_OBJECTS = {'g_fw': 0x570, 'g_rgd': 0x15498}
# Reference imports that come from GCC's static libgcc unwinder/CRT and are
# expected to vanish with -nostdlib -fno-exceptions.
GCC_RUNTIME_ONLY = {
    '_Jv_RegisterClasses', '__cxa_begin_cleanup', '__cxa_call_unexpected',
    '__cxa_finalize', '__cxa_type_match', '__deregister_frame_info',
    '__gnu_Unwind_Find_exidx', '__register_frame_info', '__div0', 'abort',
}
ABI_ATTRS = ('Tag_ABI_PCS_wchar_t', 'Tag_ABI_enum_size', 'Tag_ABI_align_needed',
             'Tag_ABI_VFP_args', 'Tag_ABI_PCS_R9_use')


def dyn_info(elf):
    dyn = elf.get_section_by_name('.dynamic')
    needed = [t.needed for t in dyn.iter_tags() if t.entry.d_tag == 'DT_NEEDED']
    soname = [t.soname for t in dyn.iter_tags() if t.entry.d_tag == 'DT_SONAME']
    textrel = any(t.entry.d_tag == 'DT_TEXTREL' for t in dyn.iter_tags())
    return needed, soname, textrel


def dynsyms(elf):
    exports, imports = set(), {}
    versions = {}
    ver = elf.get_section_by_name('.gnu.version')
    verneed = elf.get_section_by_name('.gnu.version_r')
    if verneed is not None:
        for vn, auxes in verneed.iter_versions():
            for aux in auxes:
                versions[aux.entry.vna_other] = '%s@%s' % (aux.name, vn.name)
    for i, s in enumerate(elf.get_section_by_name('.dynsym').iter_symbols()):
        if not s.name or s['st_info']['bind'] == 'STB_LOCAL':
            continue
        if s['st_shndx'] == 'SHN_UNDEF':
            idx = ver.get_symbol(i)['ndx'] if ver is not None else 0
            idx = idx if isinstance(idx, int) else 0
            imports[s.name] = versions.get(idx & 0x7fff, '')
        else:
            exports.add(s.name)
    return exports, imports


def norm(name):
    """GCC names function statics foo.1234, clang names them func.foo."""
    name = re.sub(r'\.\d+$', '', name)
    return name.rsplit('.', 1)[-1]


def objects(elf):
    sizes = {}
    for s in elf.get_section_by_name('.symtab').iter_symbols():
        if s['st_info']['type'] == 'STT_OBJECT' and s['st_size'] and s['st_shndx'] != 'SHN_UNDEF':
            sizes.setdefault(norm(s.name), Counter())[s['st_size']] += 1
    return sizes


def attrs(elf):
    out = {}
    sec = elf.get_section_by_name('.ARM.attributes')
    if sec is None:
        return out
    for sub in sec.iter_subsections():
        for sss in sub.iter_subsubsections():
            for a in sss.iter_attributes():
                out[a.tag] = a.value
    return out


def main(ref_path, new_path):
    bad = []
    with open(ref_path, 'rb') as rf, open(new_path, 'rb') as nf:
        ref, new = ELFFile(rf), ELFFile(nf)

        if ref.header.e_flags != new.header.e_flags:
            bad.append('e_flags %#x != %#x' % (new.header.e_flags, ref.header.e_flags))

        rn, rs, rt = dyn_info(ref)
        nn, ns, nt = dyn_info(new)
        print('NEEDED  ref=%s new=%s' % (rn, nn))
        print('SONAME  ref=%s new=%s' % (rs, ns))
        print('TEXTREL ref=%s new=%s' % (rt, nt))
        if sorted(rn) != sorted(nn):
            bad.append('DT_NEEDED differs')
        if rs != ns:
            bad.append('DT_SONAME differs')

        rexp, rimp = dynsyms(ref)
        nexp, nimp = dynsyms(new)
        if rexp != nexp:
            bad.append('exports differ: missing=%s extra=%s' % (sorted(rexp - nexp), sorted(nexp - rexp)))
        else:
            print('EXPORTS identical (%d): %s' % (len(nexp), ' '.join(sorted(nexp))))

        for name in sorted(set(nimp) - set(rimp)):
            if name in ALLOWED_EXTRA:
                print('IMPORT extra (allowed): %s  -- %s' % (name, ALLOWED_EXTRA[name]))
            else:
                bad.append('unexplained import: %s' % name)
        gone = sorted(set(rimp) - set(nimp))
        print('IMPORT gone (GCC runtime only): %s' % ' '.join(n for n in gone if n in GCC_RUNTIME_ONLY))
        for name in gone:
            if name in DEAD_IN_REFERENCE:
                print('IMPORT gone (dead in reference): %s  -- %s' % (name, DEAD_IN_REFERENCE[name]))
            elif name not in GCC_RUNTIME_ONLY:
                bad.append('import vanished without explanation: %s' % name)
        for name in sorted(set(rimp) & set(nimp)):
            if rimp[name] != nimp[name]:
                bad.append('import version %s: ref=%r new=%r' % (name, rimp[name], nimp[name]))
        print('IMPORTS common %d, versioned: %s' % (
            len(set(rimp) & set(nimp)),
            ' '.join(sorted('%s' % nimp[n] for n in nimp if nimp[n]))))

        ra, na = attrs(ref), attrs(new)
        for tag in ABI_ATTRS:
            if ra.get(tag) != na.get(tag):
                bad.append('%s ref=%r new=%r' % (tag, ra.get(tag), na.get(tag)))

        ro, no = objects(ref), objects(new)
        for name, size in sorted(KEY_OBJECTS.items()):
            got = dict(no.get(name, {}))
            if got != {size: 1} or dict(ro.get(name, {})) != {size: 1}:
                bad.append('key object %s expected %#x: ref=%s new=%s' % (name, size, dict(ro.get(name, {})), got))
            else:
                print('KEY %s size %#x identical' % (name, size))
        same = 0
        for name in sorted(set(ro) & set(no)):
            kind = GLOBALOPT.get(name)
            rsz = sum(s * c for s, c in ro[name].items())
            nsz = sum(s * c for s, c in no[name].items())
            if ro[name] == no[name]:
                same += 1
            elif name in INTENDED and ro[name] == Counter({INTENDED[name][0]: 1}) \
                    and no[name] == Counter({INTENDED[name][1]: 1}):
                print('INTENDED %s %d -> %d  -- %s' % (name, INTENDED[name][0], INTENDED[name][1], INTENDED[name][2]))
            elif kind == 'split' and rsz == nsz:
                print('GLOBALOPT %s split %s -> %s' % (name, dict(ro[name]), dict(no[name])))
            elif kind == 'bool' and ro[name] == Counter({4: 1}) and no[name] == Counter({1: 1}):
                print('GLOBALOPT %s int flag -> bool' % name)
            else:
                bad.append('object %s size ref=%s new=%s' % (
                    name, dict(ro[name]), dict(no[name])))
        print('OBJECTS same size: %d' % same)
        only_ref = sorted(set(ro) - set(no))
        only_new = sorted(set(no) - set(ro))
        if only_ref:
            print('OBJECTS only in reference: %s' % ' '.join(
                '%s(%s)' % (n, ','.join(map(str, ro[n]))) for n in only_ref))
        if only_new:
            print('OBJECTS only in candidate: %s' % ' '.join(
                '%s(%s)' % (n, ','.join(map(str, no[n]))) for n in only_new))

    for b in bad:
        print('MISMATCH ' + b)
    print('RGI_META_ABI_COMPARE=%s' % ('PASS' if not bad else 'FAIL'))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:3]))
