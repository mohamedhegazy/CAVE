function saveFig(f,file)
for LineWidth=2.5
for FontSize=18:2:20
	set(findall(gca, 'Type', 'Line'),'LineWidth',LineWidth);
	set(gca,'FontSize',FontSize)
	set(gca, 'FontName', 'Times New Roman');
	set(get(gca, 'xlabel'), 'interpreter', 'latex');
	set(get(gca, 'xlabel'), 'FontName', 'Times New Roman');
	set(get(gca, 'xlabel'), 'FontSize', FontSize);
	set(get(gca, 'ylabel'), 'interpreter', 'latex');
	set(get(gca, 'ylabel'), 'FontName', 'Times New Roman');
	set(get(gca, 'ylabel'), 'FontSize', FontSize);
	set(legend(), 'interpreter', 'latex');
	set(legend(), 'FontName', 'Times New Roman');
	set(legend(), 'FontSize', FontSize);
	set(gcf, 'WindowStyle', 'normal');
	set(gca, 'Unit', 'inches');
	set(gca, 'Position', [1.3 1.0 4.5 2.825]);
	set(gcf, 'Unit', 'inches');
	set(gcf, 'Position', [0.25 0.5 6.1 4.20]);
	box off
	set(legend(), 'Unit', 'inches');
	set(legend(), 'Position',[2.925 3.625 1.25 0.75]);
	set(legend(), 'Orientation','horizontal');
	legend('boxoff')
	pos = get(gcf,'Position');
	set(gcf,'PaperPositionMode','Auto','PaperUnits','Inches','PaperSize',[pos(3), pos(4)])
	% it repeats here -- matlab does not resize figure properly with only run of the script
	set(findall(gca, 'Type', 'Line'),'LineWidth',LineWidth);
	set(gca,'FontSize',FontSize)
	set(gca, 'FontName', 'Times New Roman');
	set(get(gca, 'xlabel'), 'interpreter', 'latex');
	set(get(gca, 'xlabel'), 'FontName', 'Times New Roman');
	set(get(gca, 'xlabel'), 'FontSize', FontSize);
	set(get(gca, 'ylabel'), 'interpreter', 'latex');
	set(get(gca, 'ylabel'), 'FontName', 'Times New Roman');
	set(get(gca, 'ylabel'), 'FontSize', FontSize);
	set(legend(), 'interpreter', 'latex');
	set(legend(), 'FontName', 'Times New Roman');
	set(legend(), 'FontSize', FontSize);
	set(gcf, 'WindowStyle', 'normal');
	set(gca, 'Unit', 'inches');
	set(gca, 'Position', [1.3 1.0 4.5 2.825]);
	set(gcf, 'Unit', 'inches');
	set(gcf, 'Position', [0.25 0.5 6.1 4.20]);
	box off
	set(legend(), 'Unit', 'inches');
	set(legend(), 'Position',[2.925 3.625 1.25 0.75]);
	set(legend(), 'Orientation','horizontal');
	legend('boxoff')
	pos = get(gcf,'Position');
	set(gcf,'PaperPositionMode','Auto','PaperUnits','Inches','PaperSize',[pos(3), pos(4)])
	grid on
	r=FontSize;
	z=LineWidth*2;
	print(f,char(strcat(file,'-',num2str(r),'-',num2str(z))), '-depsc')
	print(f,char(strcat(file,'-',num2str(r),'-',num2str(z))), '-dpng')
	print(f,char(strcat(file,'-',num2str(r),'-',num2str(z))), '-dpdf','-r0')	
	saveas(f,char(strcat(file,'-',num2str(r),'-',num2str(z),'.fig')),'fig');
end
end
