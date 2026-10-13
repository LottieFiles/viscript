# viscript CLI (inspect/render/layout, plus the optional layout audit).
# The executable itself is declared in the root meson.build as `vis_cli`.
vis_cli_sources = files('vis.cpp')
vis_cli_args = cpu_enabled ? ['-DVIS_ENGINE_CPU'] : []
vis_cli_args += vis_module_args
vis_cli_dependencies = [vis_dep] + vis_module_deps
vis_cli_inc = include_directories('../core/lua', '../core/savers')

if audit_enabled
    vis_cli_sources += files('visAudit.cpp')
    vis_cli_args += '-DVIS_AUDIT'
endif
