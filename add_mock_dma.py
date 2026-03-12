# Script to add mock_dma files to ArgoSentry.vcxproj

with open('ArgoSentry.vcxproj', 'r', encoding='utf-8') as f:
    lines = f.readlines()

output = []
for line in lines:
    output.append(line)
    if '<ClInclude Include="include\\ArgoSentry\\metrics.hh" />' in line:
        output.append('    <ClInclude Include="include\\ArgoSentry\\mock_dma.hh" />\n')
    elif '<ClCompile Include="src\\metrics.cpp" />' in line:
        output.append('    <ClCompile Include="src\\mock_dma.cpp" />\n')

with open('ArgoSentry.vcxproj', 'w', encoding='utf-8', newline='') as f:
    f.writelines(output)

print("✅ Successfully added mock_dma files to ArgoSentry.vcxproj")
