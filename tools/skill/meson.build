# Skill templates are checked through the CLI.
# Each entry: test-name suffix, template path, build condition, whether it
# measures text and so needs the example font ('font'), and whether to add a CPU
# render ('render') or a layout audit ('audit') test.
skill_templates = [
    {'name': '', 'src': 'vis-skills/templates/core/scene.lua'},
    {'name': '-math-shortform-circle-wave-projections', 'src': 'vis-skills/templates/math-shortform/circle-wave-projections.lua'},
    {'name': '-math-shortform-linear-space-transform', 'src': 'vis-skills/templates/math-shortform/linear-space-transform.lua'},
    {'name': '-math-shortform-tracked-tangent', 'src': 'vis-skills/templates/math-shortform/tracked-tangent.lua'},
    {'name': '-graphics-3d-surface-morph', 'src': 'vis-skills/templates/graphics/3d/surface-morph.lua', 'font': true, 'render': true, 'audit': []},
    {'name': '-interactive-button-state-motion', 'src': 'vis-interactive/templates/button-state-motion.lua', 'when': ui_enabled},
    {'name': '-interactive-ui-input-routing', 'src': 'vis-interactive/templates/ui-input-routing.lua', 'when': ui_enabled and input_enabled},
    {'name': '-input-keyboard-object-motion', 'src': 'vis-interactive/templates/keyboard-object-motion.lua', 'when': input_enabled},
    {'name': '-ai-systems-scaled-dot-product-attention', 'src': 'vis-skills/templates/ai-systems/scaled-dot-product-attention.lua', 'font': true},
    {'name': '-ai-systems-transformer-block', 'src': 'vis-skills/templates/ai-systems/transformer-block.lua', 'font': true},
    {'name': '-algorithm-and-code-binary-search', 'src': 'vis-skills/templates/algorithm-and-code/binary-search.lua'},
    {'name': '-algorithm-and-code-call-stack-and-heap', 'src': 'vis-skills/templates/algorithm-and-code/call-stack-and-heap.lua'},
    {'name': '-algorithm-and-code-merge-sort', 'src': 'vis-skills/templates/algorithm-and-code/merge-sort.lua'},
    {'name': '-diagram-structure-scenario', 'src': 'vis-skills/templates/diagram/source-code/structure-scenario.lua'},
    {
        'name': '-pipeline-plate', 'src': 'vis-skills/templates/pipeline-plate/pipeline-plate.lua',
        'font': true, 'render': true,
        'audit': [
            '--contain', 'node:load:name', 'node:load:body', '8',
            '--contain', 'node:load:when', 'node:load:body', '8',
            '--contain', 'node:update:name', 'node:update:body', '8',
            '--contain', 'node:update:when', 'node:update:body', '8',
            '--contain', 'node:draw:name', 'node:draw:body', '8',
            '--contain', 'node:draw:when', 'node:draw:body', '8',
            '--contain', 'node:screen:name', 'node:screen:body', '8',
            '--contain', 'node:screen:when', 'node:screen:body', '8',
        ],
    },
    {
        'name': '-readme-demo-command-to-result', 'src': 'vis-skills/templates/readme-demo/command-to-result.lua',
        'font': true, 'render': true,
        'audit': ['--contain', 'cmd', 'terminal', '8', '--contain', 'ok', 'terminal', '8'],
    },
    {'name': '-graphics-2d-coverage-rasterization', 'src': 'vis-skills/templates/graphics/2d/advanced/coverage-rasterization.lua'},
    {'name': '-graphics-2d-linear-gradient', 'src': 'vis-skills/templates/graphics/2d/linear-gradient.lua'},
    {'name': '-graphics-2d-sin-cos-function-plot', 'src': 'vis-skills/templates/graphics/2d/sin-cos-function-plot.lua'},
    {'name': '-graphics-2d-space-coordinates', 'src': 'vis-skills/templates/graphics/2d/space-coordinates.lua'},
    {'name': '-graphics-3d-blinn-phong', 'src': 'vis-skills/templates/graphics/3d/blinn-phong.lua'},
    {'name': '-graphics-3d-model-view-projection', 'src': 'vis-skills/templates/graphics/3d/model-view-projection.lua'},
    {'name': '-graphics-3d-quaternion-slerp', 'src': 'vis-skills/templates/graphics/3d/quaternion-slerp.lua'},
    {'name': '-diagram-causal-loop', 'src': 'vis-skills/templates/diagram/causal-loop.lua'},
    {'name': '-diagram-swimlane', 'src': 'vis-skills/templates/diagram/swimlane.lua', 'render': true},
    {
        'name': '-diagram-native', 'src': 'vis-skills/templates/diagram/native-diagram.lua',
        'when': diagram_enabled, 'render': true,
        'audit': [
            '--require-containment-ledger',
            '--allow-uncontained', 'request-flow:edge:request:label',
            '--allow-uncontained', 'request-flow:edge:dispatch:label',
            '--allow-uncontained', 'request-flow:edge:persist:label',
        ],
    },
]

foreach t : skill_templates
    if t.get('when', true)
        name = 'skill-template' + t['name']
        template = files(t['src'])
        # A template that measures text loads the fonts first.
        if t.get('font', false)
            test(name, vis_cli, args: ['inspect', template] + example_fonts)
        else
            test(name, vis_cli, args: ['inspect', template])
        endif
        if t.get('render', false)
            test(
                'render-' + name,
                vis_cli,
                args: ['render', template] + example_fonts + ['-o', name + '.png'],
            )
        endif
        if t.has_key('audit') and audit_enabled
            test('audit-' + name, vis_cli, args: ['audit', template] + example_fonts + t['audit'])
        endif
    endif
endforeach

node = find_program('node', required: false)
if node.found()
    test('skill-static-check', node, args: [files('check.mjs'), meson.project_source_root()])
    # The plate template, rendered with the bundled runtime as the VS Code Viewer renders it,
    # in Source Serif 4 and the runtime's light and dark host colors.
    test(
        'skill-host-review',
        node,
        args: [
            files('vis-vscode/scripts/host-review.mjs'),
            files('vis-skills/templates/pipeline-plate/pipeline-plate.lua'),
            '--contain', 'node:*:name', 'node:*:body', '8',
            '--contain', 'node:*:when', 'node:*:body', '8',
            '--out', meson.current_build_dir() / 'host-review',
        ],
    )
endif
