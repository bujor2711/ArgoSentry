# Script to add logging files to ArgoSentry.vcxproj

with open('ArgoSentry.vcxproj', 'r', encoding='utf-8') as f:
    lines = f.readlines()

output = []
for line in lines:
    output.append(line)
    
    # Add headers after inputstate.hh
    if '<ClInclude Include="include\\ArgoSentry\\inputstate.hh" />' in line:
        output.append('    <ClInclude Include="include\\ArgoSentry\\logger.hh" />\n')
        output.append('    <ClInclude Include="include\\ArgoSentry\\log_sinks.hh" />\n')
    
    # Add sources after inputstate.cpp
    elif '<ClCompile Include="src\\inputstate.cpp" />' in line:
        output.append('    <ClCompile Include="src\\logger.cpp" />\n')
        output.append('    <ClCompile Include="src\\file_sink.cpp" />\n')
        output.append('    <ClCompile Include="src\\console_sink.cpp" />\n')

with open('ArgoSentry.vcxproj', 'w', encoding='utf-8', newline='') as f:
    f.writelines(output)

print("✅ Successfully added logging files to ArgoSentry.vcxproj")
