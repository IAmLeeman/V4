!!ARBfp1.0
# Pass texture color straight to output
TEMP tempColor;
TEX tempColor, fragment.texcoord[0], texture[0], 2D;
MOV result.color, tempColor;
END